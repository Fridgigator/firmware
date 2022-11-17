
#include "State.h"
#include "pb_decode.h"
#include "../exceptions/DecodeException.h"
#include "../exceptions/StateException.h"
#include <memory>
#include <array>
#include <thread>
#include <cstring>
#include "../Constants.h"
#include "../GetSensorData.h"
#include "../SendData.h"
#include "../getTime.h"
#include "../lib/log.h"

using namespace std;

void printVec(optional<vector<uint8_t>> val);
void printDeque(deque<uint8_t> &val);

safe_std::mutex<WiFiState> wifiState;


State::State() : currentState(nullptr), type(GetSize) {}
std::tuple<variant<void *, int, GetCommandClass, SendWifiDataClass, GetWifiUsernamePasswordClass, GetWiFiStateSizeClass>, StateType> procGetSize(deque<uint8_t>& dequeValue){
  array<uint8_t, 4> sizeVector{};

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
  return tuple(GetCommandClass(size), GetCommand);
}

#ifdef ARDUINO
std::tuple<variant<void *, int, GetCommandClass, SendWifiDataClass, GetWifiUsernamePasswordClass, GetWiFiStateSizeClass>, StateType> getWiFi() {
  vector<WiFiStorage> wifis;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    wifis.emplace_back(WiFi.SSID(i).c_str(), WiFi.BSSID(i), WiFi.channel(i),
                       WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    delay(10);
  }

  return tuple(SendWifiDataClass(wifis), SendWifiDataSize);
}
std::tuple<variant<void *,
                   int,
                   GetCommandClass,
                   SendWifiDataClass,
                   GetWifiUsernamePasswordClass,
                   GetWiFiStateSizeClass>, StateType> wifiConnect(unique_ptr<BLESendPacket>& message){
  auto wifiConnectInfo = make_unique<WiFiConnectInfo>(message->type.wifiConnectInfo);
  isConnecting = true;
  WiFiClass::mode(WIFI_STA);
  WiFi.begin(wifiConnectInfo->ssid, wifiConnectInfo->password);
  LOG("Connecting to WiFi ..\n");
  wifiState.lockAndSwap(CONNECTING);
  std::thread th([wifiConnectInfo = std::move(wifiConnectInfo)]() {
    for (int i = 0; i < 5; i++) {
      if (WiFi.isConnected()) {
        wifiState.lockAndSwap(CONNECTED);
        Preferences preferences;
        {
          auto lock = mtxReadPrefs.lock();
          preferences.begin(WIFI_DATA_KEY);
          preferences.putString(WIFI_DATA_KEY_PASSWORD, wifiConnectInfo->password);
          preferences.putString(WIFI_DATA_KEY_SSID, wifiConnectInfo->ssid);
          preferences.end();
        }
        isConnecting = false;
        return;

      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    LOG("Timeout\n");
    wifiState.lockAndSwap(TIMEOUT);
  });
  th.detach();
  return tuple(nullptr, GetSize);
}
#endif

std::tuple<variant<void *,
                   int,
                   GetCommandClass,
                   SendWifiDataClass,
                   GetWifiUsernamePasswordClass,
                   GetWiFiStateSizeClass>, StateType> crossDevicePacket(unique_ptr<BLESendPacket> &message) {

  auto values = &message->type.crossDevicePacket.values;
  auto s = &message->type.crossDevicePacket.sensorList;
  auto timestamp = &message->type.crossDevicePacket.timestamp;
  if (*timestamp != 0 && getTime() == 0) {
    timeval tv{};

    tv.tv_sec = (int) *timestamp;
    tv.tv_usec = 0;

    settimeofday(&tv, NULL);
  }
  LOG("Getting cross device packet, values_count=%d\n", values->values_count);
  LOG("Getting cross device packet, sensor_info_count=%d\n", s->sensor_info_count);
  std::vector<std::tuple<std::string, DeviceType>> devices;
  for (int i = 0; i < s->sensor_info_count; i++) {
    LOG("\nsensor: \n  -  %s:, %d\n", s->sensor_info[i].address, s->sensor_info[i].device_type);
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
    DeviceType device_type;
    LOG("address: %s, timestamp: %ld, val: %f, type: %d",
        values->values[i].address,
        values->values[i].timestamp,
        values->values[i].value,
        values->values[i].device_type);
    switch (values->values[i].device_type) {
      case ValuesInterDevice_DEVICE_TYPE_CUSTOM:device_type = DeviceType::Custom;
        break;
      case ValuesInterDevice_DEVICE_TYPE_NORDIC:device_type = DeviceType::Nordic;
        break;
      case ValuesInterDevice_DEVICE_TYPE_HUB:device_type = DeviceType::Hub;
        break;
      case ValuesInterDevice_DEVICE_TYPE_TI:device_type = DeviceType::TI;
        break;
    }

    SensorDataStore store;
    {
      auto _sensorData = sensorData.lock();
      MeasureType measure_type;
      switch(values->values[i].measure_type){
        case ValuesInterDevice_MEASURE_TYPE_HUMIDITY: measure_type = MeasureType::HUMIDITY; break;
        case ValuesInterDevice_MEASURE_TYPE_TEMP: measure_type = MeasureType::TEMP; break;
        default:
          throw "unreachable";
      }

      auto it = _sensorData->find(values->values[i].address);
      if (it != _sensorData->end()) {
        store = SensorDataStore{
            .timestamp = values->values[i].timestamp,
            .address = values->values[i].address,
            .type = device_type,
            .value = values->values[i].value,
            .measure_type = measure_type,
        };
        // replace if found timestamp is less than sent timestamp
        if (it->second.timestamp < values->values[i].timestamp) {
          _sensorData->insert_or_assign(it->first, store);
        }
      } else {
        LOG("it->first=%s\n", values->values[i].address);
        store = SensorDataStore{
            .timestamp = values->values[i].timestamp,
            .address = values->values[i].address,
            .type = device_type,
            .value = values->values[i].value,
            .measure_type = measure_type
        };
        _sensorData->insert_or_assign(values->values[i].address, store);
      }
      LOG("\nNew Data: \n  -  %s: %f, %ld, %d\n",
          values->values[i].address,
          values->values[i].value,
          values->values[i].timestamp,
          values->values[i].device_type);
    }
  }
  return tuple(nullptr, GetSize);
}

void State::push(Reader& reader) {
  deque<uint8_t> dequeValue = reader.read();
  while (!dequeValue.empty()) {
    LOG("type: %d\n", type);

    switch (type) {
      case GetSize: {
        auto [currentStateReturn, typeReturn] = procGetSize(dequeValue);
        currentState = currentStateReturn;
        type = typeReturn;
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
            LOG("Decoding failed (get command): %s\n",PB_GET_ERROR(&stream));
            throw DecodeException();
          }
          LOG("command: %d\n", message->which_type);

          switch (message->which_type) {
            case BLESendPacket_getWifi_tag: {
              auto [currentStateReturn, typeReturn] = getWiFi();
              currentState = currentStateReturn;
              type = typeReturn;
              break;
            }
            case BLESendPacket_wifiConnectInfo_tag: {
              auto [currentStateReturn, typeReturn] = wifiConnect(message);
              currentState = currentStateReturn;
              type = typeReturn;
              break;
            }
            case BLESendPacket_getWiFiConnectState_tag: {
              type = ConnectingToWiFiStateSendSize;
              break;
            }
            case BLESendPacket_token_tag: {
              string nonce = message->type.token.uuid;
              LOG("nonce=");
              LOG("%s\n",nonce.c_str());
              // I have to do this because I'm low on stack space
              registerToken.lockAndSwap(std::move(nonce));
              break;
            }
            case BLESendPacket_crossDevicePacket_tag : {
              auto [currentStateReturn, typeReturn] = crossDevicePacket(message);
              currentState = currentStateReturn;
              type = typeReturn;
              break;
            }
            default: {
              LOG("Unknown packet type=");
              LOG("%d\n",message->which_type);
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
            LOG("Decoding failed:");
            LOG("%s\n",PB_GET_ERROR(&stream));
            throw DecodeException();
          }

          type = ConnectingToWiFiStateSendSize;

        }
        break;
      }
      default: {
        LOG("Error Pushing Type=");
        LOG("%d\n",type);
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
        type = GetSize;
        currentState = nullptr;
      }
      return returnVal.value();

    }
    case ConnectingToWiFiStateSendSize: {
      WiFiState wifiLocalState;
      {
        auto lock = wifiState.lock();
        wifiLocalState = *lock;
      }
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
        type = GetSize;
        currentState = nullptr;

      }
      return returnVal.value();
    }
    default: {
      LOG("Error Getting Type=");
      LOG("%d\n",type);
      throw StateException();
    }
  }
}

void printVec(optional<vector<uint8_t>> val) {
  if (!val.has_value()) {
    LOG("Has no value\n");
  } else {
    vector<uint8_t> v = val.value();
    for (int i = 0; i < v.size(); i++) {
      LOG("v[%d]=%d\n",i,v[i]);
    }
  }

}
void printDeque(deque<uint8_t> &val) {
  for (int i = 0; i < val.size(); i++) {
    LOG("v[%d]=%d\n",i,val[i]);
  }
}
