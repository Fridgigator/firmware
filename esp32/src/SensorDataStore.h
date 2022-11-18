#ifndef ESP32_SRC_SENSORDATASTORE_H_
#define ESP32_SRC_SENSORDATASTORE_H_
#include <string>
#include "DeviceType.h"
enum MeasureType {
  TEMP,
  HUMIDITY
};
struct SensorDataStore {
  long long timestamp;
  std::string address;
  DeviceType type;
  float value;
  MeasureType measure_type;

};

#endif //ESP32_SRC_SENSORDATASTORE_H_
