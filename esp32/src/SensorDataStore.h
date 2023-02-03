#ifndef ESP32_SRC_SENSORDATASTORE_H_
#define ESP32_SRC_SENSORDATASTORE_H_
#include <string>
#include "TypeOfDevice.h"
enum MeasureType {
  TEMP,
  HUMIDITY,
  DHT11_TEMP,
  DHT22_TEMP,
  DHT11_HUMIDITY,
  DHT22_HUMIDITY,
  PICO_TEMP,
};
struct SensorDataStore {
  long long timestamp;
  std::string address;
  TypeOfDevice type;
  float value;
  MeasureType measure_type;

};

#endif //ESP32_SRC_SENSORDATASTORE_H_
