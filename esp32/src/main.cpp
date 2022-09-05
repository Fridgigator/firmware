#include <Arduino.h>
#include "NimBLEDevice.h"
#include "WiFi.h"
#include "WiFiStorage.h"
#include "Constants.h"
#include <vector>
#include <queue>
#include <mutex>
#include <cstring>
#include <iostream>
#include <Preferences.h>

#include "StateException.h"
#include "State.h"
#include "uuid.h"

using namespace std;

NimBLECharacteristic *pRead = nullptr;
mutex mtxState;

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
      cerr << "nullptr get" << endl;
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
      cerr << "nullptr set" << endl;
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
  if(preferences.isKey("uuid")) {
    char returnUUID[37];
    UUIDGen(returnUUID);
    preferences.putString("uuid", returnUUID);
    uuid = preferences.getString("uuid").c_str();
  }
  cout<<"uuid="<<uuid<<endl;
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

  WiFi.disconnect();
  WiFi.setAutoReconnect(true);
  delay(100);
}

void loop() {
  if (WiFi.isConnected()) {
    cout << "Is connected" << endl;
  } else {
    Preferences preferences;
    preferences.begin(WIFI_DATA_KEY,true);
    if (preferences.isKey(WIFI_DATA_KEY_SSID) && preferences.isKey(WIFI_DATA_KEY_PASSWORD)) {

      WiFi.begin(preferences.getString(WIFI_DATA_KEY_SSID).c_str(), preferences.getString(WIFI_DATA_KEY_PASSWORD).c_str());
      preferences.end();

      while (!WiFi.isConnected()) {
        delay(1000);
        cout << "Trying to connect" << endl;
      }
      return;
    }
    cout << "Is not connected and cannot connect" << endl;
  }
  delay(100);
}