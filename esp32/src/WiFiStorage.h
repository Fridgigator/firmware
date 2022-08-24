#ifndef ARDUINO3_WIFISTORAGE_H
#define ARDUINO3_WIFISTORAGE_H

#include <string>
#include <vector>
#include "NimBLECharacteristic.h"

using namespace std;

class WiFiStorage {
    string SSID;
    vector<uint8_t> BSSID;
    int32_t Channel;
    bool isEncrypted;
public:
    WiFiStorage(string SSID, uint8_t *BSSID, int32_t Channel, bool isEncrypted) noexcept;

    friend class SendWifiDataClass;
};


#endif //ARDUINO3_WIFISTORAGE_H
