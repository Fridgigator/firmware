#ifndef ESP32_SRC_HTTPSEND_H_
#define ESP32_SRC_HTTPSEND_H_
#include <Arduino.h>
#include <map>
#include <variant>
#include "generated/FirmwareBackend.pb.h"

std::variant<BackendToFirmwarePacket, std::string> PostData(std::string url,
                                                            std::map<std::string, std::string> headers,
                                                            uint8_t *sendData,
                                                            size_t size);
std::variant<BackendToFirmwarePacket, std::string> GetData(std::string url,
                                                           std::map<std::string, std::string> headers);

#endif //ESP32_SRC_HTTPSEND_H_
