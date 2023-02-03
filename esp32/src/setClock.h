

#ifndef ESP32_SRC_SETCLOCK_H_
#define ESP32_SRC_SETCLOCK_H_
#include <Arduino.h>

void setClock() {
  configTime(0, 0, NTP_SERVER);

  LOG("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    LOG(".");
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo{};
  gmtime_r(&nowSecs, &timeinfo);
  LOG("Current time: ");
  LOG("%s\n",asctime(&timeinfo));
}

#endif //ESP32_SRC_SETCLOCK_H_
