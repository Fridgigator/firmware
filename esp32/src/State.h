#ifndef ESP32_STATE_H
#define ESP32_STATE_H

#include <Arduino.h>
#include <variant>
#include "NimBLEDevice.h"
#include "GetClassSize.h"
#include "SendWifiDataClass.h"
#include "GetCommandClass.h"

class State {
    enum {
        Initial, GetSize, GetCommand, SendWifiDataSize, SendWifiData
    } type;
    std::variant<void *, GetSizeClass, GetCommandClass, SendWifiDataClass> currentState;
public:
    State();

    void push(const NimBLEAttValue &ch);

    vector<uint8_t> getPacket();

};

#endif //ESP32_STATE_H
