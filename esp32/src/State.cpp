#include "State.h"
#include "generated/packet.pb.h"
#include "pb_decode.h"
#include "DecodeException.h"
#include "StateException.h"
#include <iostream>
#include <memory>
#include <WiFi.h>
#include <thread>
#include <mutex>
#include <Preferences.h>
#include "WiFiState.h"
#include "GetWiFiStateSizeClass.h"
#include "spm_headers/nanopb/pb_encode.h"
#include "Constants.h"

using namespace std;

void printVec(optional<vector<uint8_t>> val);
void printDeque(deque<uint8_t>& val);


mutex mtxWifi;
WiFiState wifiState;
State::State() {
  currentState = nullptr;
  type = Initial;
}

void State::push(const NimBLEAttValue &ch) {
  deque<uint8_t> dequeValue;
  const uint8_t *rawChPointer = ch.data();
  const uint16_t rawChSize = ch.size();
  for (int i = 0; i < rawChSize; i++) {
    dequeValue.push_back(rawChPointer[i]);
  }

  while (!dequeValue.empty()) {
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
        currentState = {GetCommandClass(GetSizeClass(sizeVector).getSize())};
        type = GetCommand;
        break;
      }
      case GetCommand: {
        auto result = std::get<GetCommandClass>(currentState).read(dequeValue);
        if (result.has_value()) {
          // Packet is huge and takes up too much stack space;
          auto message = make_unique<BLESendPacket>();
          *message = BLESendPacket_init_zero;

          pb_istream_t stream = pb_istream_from_buffer(result.value().data(), result.value().size());

          bool status = pb_decode(&stream, BLESendPacket_fields, message.get());

          if (!status) {
            cerr << "Decoding failed (get command):" << PB_GET_ERROR(&stream) << endl;
            throw DecodeException();
          }
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
              WiFiConnectInfo wifiConnectInfo = message->type.wifiConnectInfo;
              WiFiClass::mode(WIFI_STA);
              WiFi.begin(wifiConnectInfo.ssid, wifiConnectInfo.password);
              Serial.print("Connecting to WiFi ..\n");
              mtxWifi.lock();
              wifiState = CONNECTING;
              mtxWifi.unlock();
              std::thread t([wifiConnectInfo]() {
                for (int i = 0; i < 5; i++) {
                  if (WiFi.isConnected()) {

                    mtxWifi.lock();
                    wifiState = CONNECTED;
                    mtxWifi.unlock();
                    cout<<"Registering"<<endl;
                    Preferences preferences;
                    preferences.begin(WIFI_DATA_KEY);
                    preferences.putString(WIFI_DATA_KEY_PASSWORD, wifiConnectInfo.password);
                    preferences.putString(WIFI_DATA_KEY_SSID, wifiConnectInfo.ssid);


                    preferences.end();
                    cout<<"Ended Registering"<<endl;
                    return;
                  }
                  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                cout<<"Timeout"<<endl;
                mtxWifi.lock();
                wifiState = TIMEOUT;
                mtxWifi.unlock();
              });
              t.detach();
              type = Initial;
              break;
            }
            case BLESendPacket_getWiFiConnectState_tag: {
              type = ConnectingToWiFiStateSendSize;
              break;
            }
            default: {
              cerr << "Unknown packet type=" << message->which_type << endl;
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
            cerr << "Decoding failed:" << PB_GET_ERROR(&stream) << endl;
            throw DecodeException();
          }

          type = ConnectingToWiFiStateSendSize;

        }
        break;
      }
      default: {
        cerr << "Error Pushing Type=" << type << endl;
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
      cerr << "Error Getting Type=" << type << endl;
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
void printDeque(deque<uint8_t>& val) {
    for (int i = 0; i < val.size(); i++) {
      Serial.print("v[");
      Serial.print(i);
      Serial.print("]=");
      Serial.print((int) val[i]);
      Serial.print("\n");
    }

}
