#ifndef ARDUINO3_BLE_PACKET_H
#define ARDUINO3_BLE_PACKET_H

#include <Arduino.h>
#include <vector>

using namespace std;

int send(const vector<uint8_t> &vector1, NimBLECharacteristic *pCharacteristic, NimBLECharacteristic *pCharacteristic1);

#endif //ARDUINO3_BLE_PACKET_H
