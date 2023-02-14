

#ifndef ESP32_SRC_SETCLOCK_H_
#define ESP32_SRC_SETCLOCK_H_

#include "lib/ArduinoSupport/ArduinoSupport.h"

/// setClock connects to the ntp server and sets the clock
void setClock() {
    configTime(NTP_SERVER);
    LOG("Waiting for NTP time sync: ");
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2) {
        delay(500);
        LOG(".");
        yield();
        nowSecs = time(nullptr);
    }

    printf("\n");
    struct tm timeinfo{};
    gmtime_r(&nowSecs, &timeinfo);
    LOG("Current time: ");
    LOG("%s\n", asctime(&timeinfo));
}

#endif //ESP32_SRC_SETCLOCK_H_
