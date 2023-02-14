#ifndef ESP32_CONSTANTS_H
#define ESP32_CONSTANTS_H

#include "SensorDataStore.h"
#include "lib/mutex.h"
#include <memory>
#include <map>

/// uuid returns a singleton containing the dynamically generated uuid of the device
std::string* uuid();

extern safe_std::mutex<std::map<std::string, SensorDataStore>> sensorData;

/// MS_TO_STAY_CONNECTED designates how long should the device be connected to a sensor via ble
#define MS_TO_STAY_CONNECTED 15'000

/// CHARACTERISTIC_SERVER_UUID is the device's CHARACTERISTIC
#define CHARACTERISTIC_SERVER_UUID "2630acab-7bf5-4dee-97fb-af8d3955c2aa"

/// SERVICE_UUID is the device's service
#define SERVICE_UUID "170e6a4c-af9e-4a1f-843e-e4fb5e165c62"

/// REMOTE_HOST is the remote http server to connect to. We don't have TLS yet.
#define REMOTE_HOST "http://detoirhbf2f8n.cloudfront.net";

/// REMOTE_HOST is the remote websocket server to connect to. We don't have TLS yet.
#define REMOTE_HOST_WS "ws://detoirhbf2f8n.cloudfront.net"

/// NTP_SERVER is the time server from where we get the current time
#define NTP_SERVER "time.cloudflare.com"
#endif //ESP32_CONSTANTS_H
