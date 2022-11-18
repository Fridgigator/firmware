#include <ctime>
#include "Arduino.h"

#include "getTime.h"
long long getTime() {
  time_t now;
  tm timeinfo{};
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}
