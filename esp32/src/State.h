#ifndef ESP32_STATE_H
#define ESP32_STATE_H

#include <Arduino.h>
#include <variant>
#include "NimBLEDevice.h"
#include "GetSizeClass.h"
#include "SendWifiDataClass.h"
#include "GetCommandClass.h"
#include "GetWifiUsernamePasswordClass.h"
#include "GetWiFiStateSizeClass.h"

class State {
  std::variant<void *,
               GetSizeClass,
               GetCommandClass,
               SendWifiDataClass,
               GetWifiUsernamePasswordClass,
               GetWiFiStateSizeClass> currentState;
 public:
  State();

  void push(const NimBLEAttValue &ch);

  vector<uint8_t> getPacket();
  StateType type;
};

#endif //ESP32_STATE_H
