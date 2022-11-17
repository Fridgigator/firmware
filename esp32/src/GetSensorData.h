#ifndef ESP32_SRC_GETSENSORDATA_H_
#define ESP32_SRC_GETSENSORDATA_H_

#ifdef ARDUINO
#include <Arduino.h>
#include <deque>
#include <string>
#include <esp_bt_device.h>

#include "DeviceType.h"
#include "NimBLEDevice.h"
class GetSensorData {
  NimBLEClient *pClient;

 public:
  GetSensorData();
  void loop();
  void clearDevices();
  void setDevices(std::vector<std::tuple<std::string, DeviceType>> &devices);
};
extern GetSensorData *getSensorData;
#else
#include <deque>
#include <string>

#include "DeviceType.h"
class GetSensorData {

 public:
  GetSensorData();
  void loop();
  void clearDevices();
  void setDevices(std::vector<std::tuple<std::string, DeviceType>> &devices);
};
extern GetSensorData *getSensorData;

#endif

#endif //ESP32_SRC_GETSENSORDATA_H_
