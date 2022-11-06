#include "State.h"
#include "generated/packet.pb.h"
#include "pb_decode.h"
#include "DecodeException.h"
#include "StateException.h"
#include <memory>
#include <WiFi.h>
#include <thread>
#include <mutex>
#include <Preferences.h>
#include <cstring>
#include "WiFiState.h"
#include "GetWiFiStateSizeClass.h"
#include "Constants.h"
#include "GetSensorData.h"
#include "SendData.h"
#include "getTime.h"

using namespace std;

void printVec(optional<vector<uint8_t>> val);
void printDeque(deque<uint8_t> &val);

mutex mtxWifi;
WiFiState wifiState;

State::State() {
  currentState = nullptr;
  type = Initial;
}
struct StateParser {
  NimBLEAttValue *ch;
  State *t;
};
void State::push(const NimBLEAttValue &ch) {
  deque<uint8_t> dequeValue;
  const uint8_t *rawChPointer = ch.data();
  const uint16_t rawChSize = ch.size();
  Serial.printf("dequeue size=%d", rawChSize);
  for (int i = 0; i < rawChSize; i++) {
    dequeValue.push_back(rawChPointer[i]);
  }
  while (!dequeValue.empty()) {
    Serial.printf("type: %d\n", type);

    switch (type) {
      case Initial: {
        type = GetSize;
        array<uint8_t, 4> sizeVector{{}};

        sizeVector.at(0) = dequeValue.front();

        dequeValue.pop_front();
        sizeVector.at(1) = dequeValue.front();

        dequeValue.pop_front();
        sizeVector.at(2) = dequeValue.front();

        dequeValue.pop_front();
        sizeVector.at(3) = dequeValue.front();

        dequeValue.pop_front();

        int size = 0;
        std::memcpy(&size, sizeVector.data(), 4);
        currentState = {GetCommandClass(size)};
        type = GetCommand;
        break;
      }
      case GetCommand: {

        auto result = std::get<GetCommandClass>(currentState).read(dequeValue);
        if (result.has_value()) {
          // Packet is huge and takes up too much stack space;
          auto message = make_unique<BLESendPacket>();
          *message = BLESendPacket_init_zero;
          printVec(result.value());
          pb_istream_t stream = pb_istream_from_buffer(result.value().data(), result.value().size());

          bool status = pb_decode(&stream, BLESendPacket_fields, message.get());

          if (!status) {
            Serial.print("Decoding failed (get command):");
            Serial.println(PB_GET_ERROR(&stream));
            throw DecodeException();
          }
          Serial.printf("command: %d\n", message->which_type);

          switch (message->which_type) {
            case BLESendPacket_getWifi_tag: {
              vector<WiFiStorage> wifis;
              int n = WiFi.scanNetworks();
              for (int i = 0; i < n; i++) {
                wifis.emplace_back(WiFi.SSID(i).c_str(), WiFi.BSSID(i), WiFi.channel(i),
                                   WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                delay(10);
              }

              type = SendWifiDataSize;
              currentState = {SendWifiDataClass(wifis)};
              break;
            }
            case BLESendPacket_wifiConnectInfo_tag: {
              auto wifiConnectInfo = make_unique<WiFiConnectInfo>(message->type.wifiConnectInfo);
              isConnecting = true;
              WiFiClass::mode(WIFI_STA);
              WiFi.begin(wifiConnectInfo->ssid, wifiConnectInfo->password);
              Serial.print("Connecting to WiFi ..\n");
              mtxWifi.lock();
              wifiState = CONNECTING;
              mtxWifi.unlock();
              std::thread th([wifiConnectInfo = move(wifiConnectInfo)]() {
                for (int i = 0; i < 5; i++) {
                  if (WiFi.isConnected()) {

                    mtxWifi.lock();
                    wifiState = CONNECTED;
                    mtxWifi.unlock();
                    Preferences preferences;
                    mtxReadPrefs.lock();
                    preferences.begin(WIFI_DATA_KEY);
                    preferences.putString(WIFI_DATA_KEY_PASSWORD, wifiConnectInfo->password);
                    preferences.putString(WIFI_DATA_KEY_SSID, wifiConnectInfo->ssid);

                    preferences.end();
                    mtxReadPrefs.unlock();
                    isConnecting = false;
                    return;

                  }
                  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                Serial.println("Timeout");
                mtxWifi.lock();
                wifiState = TIMEOUT;
                mtxWifi.unlock();
              });
              th.detach();
              type = Initial;
              break;
            }
            case BLESendPacket_getWiFiConnectState_tag: {
              type = ConnectingToWiFiStateSendSize;
              break;
            }
            case BLESendPacket_token_tag: {
              string nonce = message->type.token.uuid;
              Serial.print("nonce=");
              Serial.println(nonce.c_str());
              // I have to do this because I'm low on stack space
              mtxRegisterToken.lock();
              registerToken = nonce;
              mtxRegisterToken.unlock();
              break;
            }
            case BLESendPacket_crossDevicePacket_tag : {

              auto values = &message->type.crossDevicePacket.values;
              auto s = &message->type.crossDevicePacket.sensorList;
              auto timestamp = &message->type.crossDevicePacket.timestamp;
              if(*timestamp != 0 && getTime() == 0) {
                timeval tv{};

                tv.tv_sec = (int)*timestamp;
                tv.tv_usec = 0;

                settimeofday(&tv,NULL);
              }
              Serial.printf("Getting cross device packet, values_count=%d\n", values->values_count);
              Serial.printf("Getting cross device packet, sensor_info_count=%d\n", s->sensor_info_count);
              std::vector<std::tuple<std::string, DeviceType>> devices;
              for (int i = 0; i < s->sensor_info_count; i++) {
                Serial.printf("\nsensor: \n  -  %s:, %d\n", s->sensor_info[i].address, s->sensor_info[i].device_type);
                string address = s->sensor_info[i].address;
                DeviceType t;
                switch (s->sensor_info[i].device_type) {
                  case SensorInfoInterDevice_DEVICE_TYPE_CUSTOM:t = DeviceType::Custom;
                    break;
                  case SensorInfoInterDevice_DEVICE_TYPE_NORDIC:t = DeviceType::Nordic;
                    break;
                  case SensorInfoInterDevice_DEVICE_TYPE_HUB:t = DeviceType::Hub;
                    break;
                  case SensorInfoInterDevice_DEVICE_TYPE_TI:t = DeviceType::TI;
                    break;
                }
                devices.emplace_back(std::tuple(address, t));
              }
              getSensorData->setDevices(devices);
              for (int i = 0; i < values->values_count; i++) {
                DeviceType t;
                Serial.printf("address: %s, timestamp: %lld, val: %f, type: %d",
                              values->values[i].address,
                              values->values[i].timestamp,
                              values->values[i].value,
                              values->values[i].device_type);
                switch (values->values[i].device_type) {
                  case ValuesInterDevice_DEVICE_TYPE_CUSTOM:t = DeviceType::Custom;
                    break;
                  case ValuesInterDevice_DEVICE_TYPE_NORDIC:t = DeviceType::Nordic;
                    break;
                  case ValuesInterDevice_DEVICE_TYPE_HUB:t = DeviceType::Hub;
                    break;
                  case ValuesInterDevice_DEVICE_TYPE_TI:t = DeviceType::TI;
                    break;
                }

                bool shouldSend = false;
                SensorDataStore store;

                sensorDataMutex.lock();
                auto it = sensorData.find(values->values[i].address);
                if (it != sensorData.end()) {
                  store = SensorDataStore{
                      .timestamp = values->values[i].timestamp,
                      .address = values->values[i].address,
                      .type = t,
                      .value = values->values[i].value,
                  };
                  // replace if found timestamp is less than sent timestamp
                  if (it->second.timestamp < values->values[i].timestamp) {
                    sensorData.insert_or_assign(it->first, store);
                    shouldSend = true;
                  }
                } else {
                  Serial.printf("it->first=%s\n", values->values[i].address);
                  store = SensorDataStore{
                      .timestamp = values->values[i].timestamp,
                      .address = values->values[i].address,
                      .type = t,
                      .value = values->values[i].value,
                  };
                  sensorData.insert_or_assign(values->values[i].address, store);

                  shouldSend = true;
                }
                Serial.printf("\nNew Data: \n  -  %s: %f, %lld, %d\n",
                              values->values[i].address,
                              values->values[i].value,
                              values->values[i].timestamp,
                              values->values[i].device_type);
                sensorDataMutex.unlock();
              }
              break;
            }
            default: {
              Serial.print("Unknown packet type=");
              Serial.println(message->which_type);
              throw DecodeException();
            }
          }
        }
        break;
      }
      case GetWifiUsernamePasswordSize: {

        array<uint8_t, 4> sizeVector{{}};

        sizeVector.at(0) = dequeValue.front();
        dequeValue.pop_front();
        sizeVector.at(1) = dequeValue.front();
        dequeValue.pop_front();
        sizeVector.at(2) = dequeValue.front();
        dequeValue.pop_front();
        sizeVector.at(3) = dequeValue.front();
        dequeValue.pop_front();

        type = ConnectingToWiFiStateSendSize;
        break;
      }
      case GetWifiUsernamePassword: {
        auto result = std::get<GetCommandClass>(currentState).read(dequeValue);
        if (result.has_value()) {
          // Packet is huge and takes up too much stack space
          auto message = make_unique<WiFiConnectInfo>();
          message->ssid[0] = 0;
          message->password[0] = 0;

          pb_istream_t stream = pb_istream_from_buffer(result.value().data(), result.value().size());

          bool status = pb_decode(&stream, WiFiConnectInfo_fields, message.get());

          if (!status) {
            Serial.print("Decoding failed:");
            Serial.println(PB_GET_ERROR(&stream));
            throw DecodeException();
          }

          type = ConnectingToWiFiStateSendSize;

        }
        break;
      }
      default: {
        Serial.print("Error Pushing Type=");
        Serial.println(type);
        printDeque(dequeValue);
        throw StateException();
      }
    }
  }
}

vector<uint8_t> State::getPacket() {
  switch (type) {
    case SendWifiDataSize: {
      auto wifiData = get<SendWifiDataClass>(currentState);
      currentState = {wifiData};
      vector<uint8_t> size = wifiData.getSize();
      type = SendWifiData;
      return size;

    }
    case SendWifiData: {
      auto wifiData = get<SendWifiDataClass>(currentState);
      auto returnVal = wifiData.getData(126);
      currentState = {wifiData};

      if (wifiData.isEmpty()) {
        type = Initial;
        currentState = nullptr;
      }
      return returnVal.value();

    }
    case ConnectingToWiFiStateSendSize: {
      mtxWifi.lock();
      auto wifiLocalState = wifiState;
      mtxWifi.unlock();
      auto nextState = GetWiFiStateSizeClass(wifiLocalState);
      type = ConnectingToWiFiState;
      currentState = {nextState};

      return nextState.getSize();
    }
    case ConnectingToWiFiState: {
      auto ws = get<GetWiFiStateSizeClass>(currentState);

      auto returnVal = ws.getData(126);
      currentState = {ws};

      if (ws.isEmpty()) {
        type = Initial;
        currentState = nullptr;

      }
      return returnVal.value();
    }
    default: {
      Serial.print("Error Getting Type=");
      Serial.println(type);
      throw StateException();
    }
  }
}

void printVec(optional<vector<uint8_t>> val) {
  if (!val.has_value()) {
    Serial.print("Has no value\n");
  } else {
    vector<uint8_t> v = val.value();
    for (int i = 0; i < v.size(); i++) {
      Serial.print("v[");
      Serial.print(i);
      Serial.print("]=");
      Serial.print((int) v[i]);
      Serial.print("\n");
    }
  }

}
void printDeque(deque<uint8_t> &val) {
  for (int i = 0; i < val.size(); i++) {
    Serial.print("v[");
    Serial.print(i);
    Serial.print("]=");
    Serial.print((int) val[i]);
    Serial.print("\n");
  }

}
