#ifndef ESP32_SRC_SENSORDATASTORE_H_
#define ESP32_SRC_SENSORDATASTORE_H_
#include <string>
#include "DeviceType.h"
struct SensorDataStore {
  long long timestamp;
  std::string address;
  DeviceType type;
  float value;
};


#endif //ESP32_SRC_SENSORDATASTORE_H_
