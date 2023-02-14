#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef ESP32_ARDUINOSUPPORT_H
#define ESP32_ARDUINOSUPPORT_H

/// delay puts the current thread to sleep for ms milliseconds
void delay(uint32_t ms);

/// configTime connects to the given ntp server and sets the current time
void configTime(const char *server1);

/// yield gives back control to the scheduler to let it schedule the next process
void yield();

#endif //ESP32_ARDUINOSUPPORT_H
