#ifndef ESP32_CONSTANTS_H
#define ESP32_CONSTANTS_H

#include "SensorDataStore.h"
#include "lib/mutex.h"
#include <memory>
#include <map>


extern std::string uuid;
extern bool isConnecting;
extern safe_std::mutex<std::map<std::string, SensorDataStore>> sensorData;

#define CHARACTERISTIC_SERVER_UUID "2630acab-7bf5-4dee-97fb-af8d3955c2aa"
#define SERVICE_UUID "170e6a4c-af9e-4a1f-843e-e4fb5e165c62"
#define REMOTE_HOST "http://ec2-3-84-82-97.compute-1.amazonaws.com"
#define REMOTE_HOST_WS "ws://ec2-3-84-82-97.compute-1.amazonaws.com"
#define NTP_SERVER "time.cloudflare.com"
#endif //ESP32_CONSTANTS_H
