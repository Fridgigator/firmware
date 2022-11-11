#ifndef ESP32_STATE_H
#define ESP32_STATE_H

#include <Arduino.h>
#include <variant>
#include "NimBLEDevice.h"
#include "SendWifiDataClass.h"
#include "GetCommandClass.h"
#include "GetWifiUsernamePasswordClass.h"
#include "GetWiFiStateSizeClass.h"
#include "StateType.h"

class State {
  std::variant<void *,
               int,
               GetCommandClass,
               SendWifiDataClass,
               GetWifiUsernamePasswordClass,
               GetWiFiStateSizeClass> currentState{};
 public:
  State();

  void push(const NimBLEAttValue &ch);

  vector<uint8_t> getPacket();
  StateType type;
};

#endif //ESP32_STATE_H
