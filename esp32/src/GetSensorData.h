#ifndef ESP32_SRC_GETSENSORDATA_H_
#define ESP32_SRC_GETSENSORDATA_H_
#include <Arduino.h>
#include <deque>
#include <string>
#include <esp_bt_device.h>
#include <mutex>

#include "DeviceType.h"
#include "NimBLEDevice.h"
class GetSensorData {
  std::deque<std::tuple<std::string, DeviceType>> addresses;
  NimBLEClient *pClient;
  std::mutex mtxGetBLEAddress;

 public:
  GetSensorData();
  void loop();
  void addDevice(const std::string &name, DeviceType deviceType);
  void clearDevices();

};

#endif //ESP32_SRC_GETSENSORDATA_H_
