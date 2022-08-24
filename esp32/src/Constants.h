#ifndef ESP32_CONSTANTS_H
#define ESP32_CONSTANTS_H

#include <mutex>

const unsigned int ACK = 4294967295;
extern mutex mtxState;
extern bool isConnected;
#define CHARACTERISTIC_UUID "2630acab-7bf5-4dee-97fb-af8d3955c2aa"
#define SERVICE_UUID "170e6a4c-af9e-4a1f-843e-e4fb5e165c62"

#endif //ESP32_CONSTANTS_H
