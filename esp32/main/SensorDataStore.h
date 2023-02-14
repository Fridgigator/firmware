#ifndef ESP32_SRC_SENSORDATASTORE_H_
#define ESP32_SRC_SENSORDATASTORE_H_

#include <string>
#include "TypeOfDevice.h"

/// MeasureType identifies the type of sensor and the type of measurement obtained from a remote device.
enum MeasureType {
    TEMP, HUMIDITY, DHT11_TEMP, DHT22_TEMP, DHT11_HUMIDITY, DHT22_HUMIDITY, PICO_TEMP,
};

/// SensorDataStore holds a single data measurement
struct SensorDataStore {
    /// timestamp is a unix timestamp in UTC
    long long timestamp;
    /// address is a BLE address
    std::string address;
    /// type is a device type
    TypeOfDevice type;
    /// value holds the measured value (of unit specified in measure_type)
    float value;
    /// measure_type holds the type of measurement
    MeasureType measure_type;

};

#endif //ESP32_SRC_SENSORDATASTORE_H_
