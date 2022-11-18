#ifndef ESP32_CONSTANTS_H
#define ESP32_CONSTANTS_H

#include "SensorDataStore.h"
#include "lib/mutex.h"
#include <memory>
#include <map>
#include "state/State.h"

extern safe_std::mutex<State *> state;
extern safe_std::mutex<std::string> registerToken;

extern std::string uuid;
extern safe_std::mutex<bool> mtxReadPrefs;
extern bool isConnecting;
extern safe_std::mutex<std::map<std::string, SensorDataStore>> sensorData;

#define CHARACTERISTIC_SERVER_UUID "2630acab-7bf5-4dee-97fb-af8d3955c2aa"
#define SERVICE_UUID "170e6a4c-af9e-4a1f-843e-e4fb5e165c62"
#define WIFI_DATA_KEY "WIFIDATAKEY"
#define WIFI_DATA_KEY_SSID "SSID"
#define WIFI_DATA_KEY_PASSWORD "PASSWORD"
#endif //ESP32_CONSTANTS_H
