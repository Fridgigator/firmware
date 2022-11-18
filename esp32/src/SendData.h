#ifndef ESP32_SRC_SENDDATA_H_
#define ESP32_SRC_SENDDATA_H_
#include "Arduino.h"
#include <string>
#include <map>
#include "HTTPSend.h"
#include "SensorDataStore.h"

void SendData(const std::string &remoteAddress, const SensorDataStore &store);

#endif //ESP32_SRC_SENDDATA_H_
