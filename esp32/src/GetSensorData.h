#ifndef ESP32_SRC_GETSENSORDATA_H_
#define ESP32_SRC_GETSENSORDATA_H_

#include <Arduino.h>
#include <deque>
#include <string>
#include <esp_bt_device.h>
#include <optional>
#include "TypeOfDevice.h"
#include "NimBLEDevice.h"
class GetSensorData {
  NimBLEClient *pClient;

 public:
  GetSensorData();
  void loop();
  void clearDevices();
  void setDevices(std::vector<std::tuple<std::string, TypeOfDevice>> &devices);
};
extern std::optional<GetSensorData> getSensorData;
#endif //ESP32_SRC_GETSENSORDATA_H_
