#include <Arduino.h>
#include "NimBLEDevice.h"
#include "WiFi.h"
#include <vector>
#include <queue>
#include <mutex>
#include <cstring>
#include <Preferences.h>
#include <HTTPClient.h>
#include <thread>
#include <map>

#include "StateException.h"
#include "State.h"
#include "uuid.h"
#include "WiFiStorage.h"
#include "Constants.h"
#include "GetSensorData.h"
#include "WebDataPollingClient.h"
#include "pb_encode.h"
#include "generated/FirmwareBackend.pb.h"
#include "pb_decode.h"
#include "DecodeException.h"
#include "BLEUtils.h"
#include "setClock.h"
#include "HTTPSend.h"

using namespace std;

NimBLECharacteristic *pRead = nullptr;
mutex mtxState;
mutex mtxReadPrefs;

mutex mtxRegisterToken;
string registerToken;

mutex mtxBleIsInUse;
bool bleIsInUse = false;

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
    mtxBleIsInUse.lock();
    bleIsInUse = true;
    mtxBleIsInUse.unlock();
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
    mtxBleIsInUse.lock();
    bleIsInUse = false;
    mtxBleIsInUse.unlock();
  }

};

vector<uint8_t> getData(NimBLECharacteristic *pRead, NimBLECharacteristic *pWrite);
WebData::WebDataPollingClient *dataFromServerClient;
CharacteristicCallback *c_callback;
GetSensorData *sensorData;

void loop1();
void clientConnectLoop();
string uuid;

void recData(BackendToFirmwarePacket packet) {

  switch (packet.which_type) {
    case BackendToFirmwarePacket_get_sensors_list_tag: {
      ScanResults scanResultsClass;
      auto res = scanResultsClass.getScanResults();
      vector<SensorInfo> sensorInfo;
      for (int i = 0; i < res.getCount(); i++) {
        auto device = res.getDevice(i);
        string addressString = device.getAddress().toString();
        string nameString = device.getName();
        SensorInfo info{};
        int len = 0;
        for (int j = 0; j < addressString.size() && j < sizeof(info.address) / sizeof(info.address[0]);
             j++) {
          info.address[j] = addressString.at(j);
          len = j;

        }
        info.address[len + 1] = 0;
        len = 0;

        for (int j = 0; j < nameString.size() && j < sizeof(info.name) / sizeof(info.name[0]); j++) {
          info.name[j] = nameString.at(j);
          len = j;
        }
        info.name[len + 1] = 0;
        Serial.printf(" - name=%s", info.name);
        sensorInfo.push_back(info);
      }
      res.getCount();
      auto buf = new pb_byte_t[1024];
      pb_ostream_t output = pb_ostream_from_buffer(buf, 1024);
      SensorsList sensorsList{
          .sensor_info_count = static_cast<pb_size_t>(sensorInfo.size()),
      };

      Serial.printf("sizeof sensorlist = %d\n", sizeof(sensorsList.sensor_info) / sizeof(sensorsList.sensor_info[0]));
      for (int i = 0; i < sensorInfo.size()
          && i < sizeof(sensorsList.sensor_info) / sizeof(sensorsList.sensor_info[0]); i++) {

        Serial.printf(" - address=%s\n", sensorInfo.at(i).address);
        Serial.printf(" - name=%s\n", sensorInfo.at(i).name);
        sensorsList.sensor_info[i] = sensorInfo.at(i);

      }
      int encodeResult = pb_encode(&output, SensorsList_fields, &sensorsList);
      if (!encodeResult) {
        Serial.print("Encoding failed:");
        Serial.println(PB_GET_ERROR(&output));
        throw DecodeException();
      }
      std::map<string, string> headers;
      headers.emplace("Board", uuid);
      std::string url = "/api/send-sensors";
      Serial.printf("buf size=%d\n", output.bytes_written);
      PostData(url, headers, buf, output.bytes_written);
      delete[] buf;
      break;
    }
    case BackendToFirmwarePacket_clear_sensor_list_tag: {

      sensorData->clearDevices();

      break;
    }
    case BackendToFirmwarePacket_add_sensor_tag: {
      vector<std::tuple<std::string, DeviceType>> newDevices;
      for (int i = 0; i < packet.type.add_sensor.add_sensor_info_count; i++) {
        auto addSensorInfo = packet.type.add_sensor.add_sensor_info[i];
        std::string address = addSensorInfo.sensor_info.address;
        DeviceType deviceType;
        switch (addSensorInfo.device_type) {
          case AddSensorInfo_DEVICE_TYPE_TI:deviceType = DeviceType::TI;

            break;
          case AddSensorInfo_DEVICE_TYPE_NORDIC:deviceType = DeviceType::Nordic;
            break;
          case AddSensorInfo_DEVICE_TYPE_CUSTOM:deviceType = DeviceType::Custom;
            break;
        }
        newDevices.emplace_back(std::tuple(address, deviceType));
      }
      sensorData->setDevices(newDevices);
      break;
    }
    default: {
      break;
    }
  }
}

[[noreturn]]
void outerLoop1(void *arg) {
  for (;;) {
    loop1();
    delay(100);
  }

}
void setup() {
  WiFiClass::mode(WIFI_STA);
  sensorData = new GetSensorData();

  Serial.begin(115200);

  Serial.println("Starting BLE work!");
  c_callback = new CharacteristicCallback();
  NimBLEDevice::init("ESP32");
  Preferences preferences;
  preferences.begin("permanent", false);
  uuid = preferences.getString("uuid").c_str();
  if (!preferences.isKey("uuid")) {
    char returnUUID[37];
    UUIDGen(returnUUID);
    preferences.putString("uuid", returnUUID);
    uuid = preferences.getString("uuid").c_str();
  }
  Serial.printf("uuid=%s\n", uuid.c_str());
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

  auto *pServerCallbacks = new ServerCallbacks();
  pServer->setCallbacks(pServerCallbacks);

  Serial.println("finished adding devices");
  thread t([]() __attribute__((noreturn)) {
    for (;;) {
      mtxBleIsInUse.lock();
      bool _bleIsInUse = bleIsInUse;
      mtxBleIsInUse.unlock();
      if (!_bleIsInUse) {
        sensorData->loop();
      }
      delay(10'000);

    }
  });
  t.detach();
  delay(100);
  TaskHandle_t Task1;

  xTaskCreate(outerLoop1, "Main Loop Task", 16000, (void *) nullptr, 1, &Task1);

  Serial.println("Characteristic defined! Now you can read it in your phone!");

}
void clientConnectLoop() {
  TaskHandle_t Task1;
  xTaskCreate([](void *parameters) {
    auto headersMap = std::map<string, string>();
    headersMap.emplace("Board", uuid.c_str());
    string url = "/api/hub-connect";
    WebData::WebDataPollingClient dataFromServerClient(url, headersMap);
    dataFromServerClient.onRecData(recData);
    for (;;) {
      dataFromServerClient.poll();
      delay(2000);
    }
  }, "Name", 32000, (void *) nullptr, 1, &Task1);

  Serial.println("Finished creating connection");
}

bool timeSet = false;
int tryingToConnect = 0;
bool isConnecting = false;

void loop1() {
  Serial.printf("Wifi is connected: %d", WiFi.isConnected());

  if (WiFi.isConnected()) {
    Serial.println("is connected");

    if (!timeSet) {
      Serial.println("setting clock");
      setClock();
      clientConnectLoop();

      timeSet = true;
    }
    mtxRegisterToken.lock();
    auto _registerToken = registerToken;
    registerToken = "";
    mtxRegisterToken.unlock();

    if (!_registerToken.empty()) {
      Serial.println("token register");

      String url = ("http://fridgigator.herokuapp.com/api/register-hub?hub-uuid=");
      url += (uuid.c_str());
      TaskHandle_t Task1;
      HTTPClient http_client;
      Serial.printf("url: %s\n", url.c_str());
      bool begin = http_client.begin(url/*, rootCACertificate*/);
      Serial.printf("begin: %d \n", begin);
      http_client.addHeader("Authorization", _registerToken.c_str());
      log_d("Free internal heap before TLS %u", ESP.getFreeHeap());
      log_d("Free PSRAM %u", ESP.getFreePsram());
      int res = http_client.GET();
      Serial.printf("res: %d\n", res);

      int httpCode = res;
      if (httpCode < 0) {
        Serial.print(" main -  get failed: ");
        Serial.println(HTTPClient::errorToString(httpCode));

      }
      http_client.end();
    }

  } else {
    Serial.println("WiFi is not connected");
    Preferences preferences;
    mtxReadPrefs.lock();
    preferences.begin(WIFI_DATA_KEY, true);
    bool hasKey = preferences.isKey(WIFI_DATA_KEY_SSID) && preferences.isKey(WIFI_DATA_KEY_PASSWORD);
    preferences.end();
    mtxReadPrefs.unlock();
    Serial.printf("has key=%d\n", hasKey);

    if (hasKey) {
      while (!WiFi.isConnected() && !isConnecting) {
        preferences.begin(WIFI_DATA_KEY, true);
        mtxReadPrefs.lock();
        auto key = preferences.getString(WIFI_DATA_KEY_SSID);
        auto pass = preferences.getString(WIFI_DATA_KEY_PASSWORD);
        Serial.printf("%s: %s\n", key.c_str(), pass.c_str());

        preferences.end();
        mtxReadPrefs.unlock();

        wl_status_t status = WiFi.status();
        Serial.printf("status=%d\n", status);

        WiFi.begin(key.c_str(),
                   pass.c_str());

        delay(10000);
        status = WiFi.status();
        Serial.printf("status=%d\n", status);

        Serial.printf("Trying to connect, key=%s, pass=%s", key.c_str(), pass.c_str());
        if (tryingToConnect == 1000) {
          Serial.println("Tried too long to connect. Restarting \n\n");
          esp_restart();
        }
        tryingToConnect++;
      }
      return;
    }
    Serial.println("Is not connected and cannot connect");
  }

  delay(100);

}

void loop() {}