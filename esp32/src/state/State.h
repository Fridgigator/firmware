#ifndef ESP32_STATE_H
#define ESP32_STATE_H
#ifdef ARDUINO
#include <WiFi.h>
#include <Preferences.h>
#else
int settimeofday(const struct timeval *tv, const struct timezone *tz);
#endif

#include <variant>
#include <condition_variable>
#include "../SendWifiDataClass.h"
#include "../GetCommandClass.h"
#include "../GetWifiUsernamePasswordClass.h"
#include "../GetWiFiStateSizeClass.h"
#include "StateType.h"
#include "Reader.h"
#include "../generated/packet.pb.h"

typedef std::tuple<variant<void *,
                           int,
                           GetCommandClass,
                           SendWifiDataClass,
                           GetWifiUsernamePasswordClass,
                           GetWiFiStateSizeClass>, StateType> (*x)(unique_ptr<BLESendPacket> &);
std::tuple<variant<void *,
                   int,
                   GetCommandClass,
                   SendWifiDataClass,
                   GetWifiUsernamePasswordClass,
                   GetWiFiStateSizeClass>, StateType> procGetSize(deque<uint8_t> &dequeValue);
std::tuple<variant<void *,
                   int,
                   GetCommandClass,
                   SendWifiDataClass,
                   GetWifiUsernamePasswordClass,
                   GetWiFiStateSizeClass>, StateType> getWiFi();
std::tuple<variant<void *,
                   int,
                   GetCommandClass,
                   SendWifiDataClass,
                   GetWifiUsernamePasswordClass,
                   GetWiFiStateSizeClass>, StateType> wifiConnect(unique_ptr<BLESendPacket> &ptr);
class State {
  std::variant<void *,
               int,
               GetCommandClass,
               SendWifiDataClass,
               GetWifiUsernamePasswordClass,
               GetWiFiStateSizeClass> currentState{};
 public:
  State();
  void push(Reader &reader);
  vector<uint8_t> getPacket();
  StateType type;
};

#endif //ESP32_STATE_H
