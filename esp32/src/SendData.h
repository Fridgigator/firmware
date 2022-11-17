#ifndef ESP32_SRC_SENDDATA_H_
#define ESP32_SRC_SENDDATA_H_
#ifdef ARDUINO
#include "Arduino.h"
#include <string>
#include <map>
#include "HTTPSend.h"
#include "SensorDataStore.h"

void SendData(const std::string& remoteAddress, const SensorDataStore& store);
#else
#include <string>
#include <map>

void SendData(const std::string& remoteAddress, const SensorDataStore& store);

#endif

#endif //ESP32_SRC_SENDDATA_H_
