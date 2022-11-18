#ifndef ESP32_SRC_LIB_LOG_H_
#define ESP32_SRC_LIB_LOG_H_

#ifdef ARDUINO
#include <Arduino.h>
#define LOG(format, args...) Serial.printf(format, ## args)
#else
#define LOG(format,args...) printf(format, ## args);
#endif
#endif //ESP32_SRC_LIB_LOG_H_
