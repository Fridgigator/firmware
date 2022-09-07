#include <Arduino.h>
#include "NimBLEDevice.h"
#include "WiFi.h"
#include <vector>
#include <queue>
#include <mutex>
#include <cstring>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoWebsockets.h>
#include <thread>

#include "StateException.h"
#include "State.h"
#include "uuid.h"
#include "WiFiStorage.h"
#include "Constants.h"

using namespace std;

NimBLECharacteristic *pRead = nullptr;
mutex mtxState;

mutex mtxRegisterToken;
string registerToken;

bool isConnected;
State *s;

class CharacteristicCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *ch) override {
    if (s != nullptr) {
      mtxState.lock();

      auto val = ch->getValue();
      s->push(val);
      mtxState.unlock();
    } else {
      Serial.println("nullptr get");
      throw StateException();

    }
  }

  void onRead(NimBLECharacteristic *ch) override {
    if (s != nullptr) {
      mtxState.lock();

      ch->setValue(s->getPacket());
      ch->indicate();
      mtxState.unlock();
    } else {
      Serial.println("nullptr set");
      throw StateException();
    }

  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) override {
    mtxState.lock();
    delete s;
    s = new State();
    mtxState.unlock();

  }

  void onDisconnect(NimBLEServer *pServer) override {
    mtxState.lock();
    delete s;
    s = nullptr;
    mtxState.unlock();
  }

};

vector<uint8_t> getData(NimBLECharacteristic *pRead, NimBLECharacteristic *pWrite);
websockets::WebsocketsClient client;

CharacteristicCallback *c_callback;
void setup() {
  WiFiClass::mode(WIFI_STA);

  Serial.begin(115200);

  Serial.println("Starting BLE work!");
  c_callback = new CharacteristicCallback();
  NimBLEDevice::init("ESP32");
  Preferences preferences;
  preferences.begin("permanent", false);
  string uuid = preferences.getString("uuid").c_str();
  if(!preferences.isKey("uuid")) {
    char returnUUID[37];
    UUIDGen(returnUUID);
    preferences.putString("uuid", returnUUID);
    uuid = preferences.getString("uuid").c_str();
  }
  Serial.println("uuid=");
  Serial.println(uuid.c_str());
  preferences.end();
  NimBLEServer *pServer = BLEDevice::createServer();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pRead = pService->createCharacteristic(CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
  pRead->setCallbacks(c_callback);
  pService->start();
  NimBLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  NimBLEDevice::startAdvertising();
  NimBLEDevice::stopAdvertising();
  NimBLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
  auto *pServerCallbacks = new ServerCallbacks();
  pServer->setCallbacks(pServerCallbacks);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  auto websocketPingThread = thread([]{
    try {
      while (true) {
        Serial.print("poll");
        client.poll();
        delay(1000);
      }
    }catch (...) {
      Serial.println("Done poll");
    }
  });
  websocketPingThread.detach();
  client.addHeader("Board", uuid.c_str());
  client.onMessage([](const websockets::WebsocketsMessage& msg){
    Serial.print("I got a message: ");
    Serial.println(msg.c_str());
  });
  client.onEvent([](websockets::WebsocketsEvent event, const String& data){
    if(event == websockets::WebsocketsEvent::ConnectionOpened) {
      Serial.println("Connection Opened");
    } else if(event == websockets::WebsocketsEvent::ConnectionClosed) {
      Serial.println("Connection Closed");
    } else if(event == websockets::WebsocketsEvent::GotPing) {
      Serial.println("Got a Ping!");
    } else if(event == websockets::WebsocketsEvent::GotPong) {
      Serial.println("Got a Pong!");
    }
    Serial.print("Data=");
    Serial.println(data);
  });
  client.setCACert(rootCACertificate);
  client.connect("wss://fridgigator.herokuapp.com/api/hub-websocket");

  delay(100);
}
bool timeSet = false;
int tryingToConnect = 0;
void setClock();
void loop() {
  Serial.print("Is client available?");
  Serial.print(client.available());

  if (WiFi.isConnected()) {
    if(!client.available()){
      auto result = client.connect("fridgigator.herokuapp.com",443,"/api/hub-websocket");
      Serial.print("Connection to websocket: ");
      Serial.println(result);
    }
    Preferences preferences;
    preferences.begin("permanent", false);
    auto uuid = string(preferences.getString("uuid").c_str());
    preferences.end();

    Serial.println("Is connected");
    if(!timeSet) {
      setClock();
      timeSet = true;
    }
    mtxRegisterToken.lock();
    auto _registerToken = registerToken;
    registerToken = "";
    mtxRegisterToken.unlock();
    if(!_registerToken.empty()) {
      HTTPClient http_client;
      string url = string("https://fridgigator.herokuapp.com/api/register-hub?hub-name=")+uuid;
      http_client.begin(url.c_str(), rootCACertificate);
      http_client.addHeader("Authorization", _registerToken.c_str());
      Serial.println("main - About to get");
      Serial.println(ESP.getFreeHeap());
      int httpCode = http_client.GET();
      Serial.println(" main -  Got");
      Serial.println(std::to_string(httpCode).c_str());
      if (httpCode < 0) {
        Serial.print(" main -  get failed: ");
        Serial.println(HTTPClient::errorToString(httpCode));

      }
    }

  } else {
    Preferences preferences;
    preferences.begin(WIFI_DATA_KEY,true);
    if (preferences.isKey(WIFI_DATA_KEY_SSID) && preferences.isKey(WIFI_DATA_KEY_PASSWORD)) {
      Serial.println(preferences.getString(WIFI_DATA_KEY_SSID).c_str());

      WiFi.begin(preferences.getString(WIFI_DATA_KEY_SSID).c_str(), preferences.getString(WIFI_DATA_KEY_PASSWORD).c_str());
      preferences.end();

      while (!WiFi.isConnected()) {
        delay(1000);
        Serial.println("Trying to connect");
        if(tryingToConnect == 100) {
          esp_restart();
        }
        tryingToConnect++;
      }
      return;
    }
    Serial.println("Is not connected and cannot connect");
  }
  Serial.println(ESP.getFreeHeap());
  delay(1000);
}
void setClock() {
  configTime(0, 0, "pool.ntp.org");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo{};
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}