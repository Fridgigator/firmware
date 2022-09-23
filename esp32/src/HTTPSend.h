#ifndef ESP32_SRC_HTTPSEND_H_
#define ESP32_SRC_HTTPSEND_H_
#include <Arduino.h>
#include <map>
#include "generated/FirmwareBackend.pb.h"

BackendToFirmwarePacket PostData(std::string &url, std::map<std::string, std::string> &headers, uint8_t *sendData, size_t size);
BackendToFirmwarePacket GetData(std::string &url, std::map<std::string, std::string> &headers);

#endif //ESP32_SRC_HTTPSEND_H_
