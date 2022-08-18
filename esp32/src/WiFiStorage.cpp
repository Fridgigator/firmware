#include <iostream>
#include "WiFiStorage.h"

WiFiStorage::WiFiStorage(string SSID, uint8_t *BSSID, int32_t Channel, bool isEncrypted) noexcept {
    this->SSID = SSID;
    if(BSSID != nullptr) {
        for(int i = 0;i<6;i++) {
            this->BSSID.push_back(BSSID[i]);
        }
    }
    this->Channel = Channel;
    this->isEncrypted = isEncrypted;
}
