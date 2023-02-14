#ifndef ESP32_SRC_LIB_LOG_H_
#define ESP32_SRC_LIB_LOG_H_
#define LOG(format, args...) printf(format, ## args);
#endif //ESP32_SRC_LIB_LOG_H_
