#include <Arduino.h>
#include "NimBLEDevice.h"
#include <string>
#include "WiFi.h"
#include "WiFiStorage.h"
#include "BLE_packet.h"
#include <vector>

using namespace std;

#define SERVICE_UUID              "170e6a4c-af9e-4a1f-843e-e4fb5e165c62"
#define CHARACTERISTIC_UUID_READ  "2630acab-7bf5-4dee-97fb-af8d3955c2aa"
#define CHARACTERISTIC_UUID_WRITE "e84d9cc1-c565-4f05-85a2-3a32ebc978b6"

NimBLECharacteristic *pRead = nullptr;
NimBLECharacteristic *pWrite = nullptr;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    NimBLEDevice::init("Long name works now");
    NimBLEServer *pServer = BLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    pRead = pService->createCharacteristic(
            CHARACTERISTIC_UUID_READ,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pWrite = pService->createCharacteristic(
            CHARACTERISTIC_UUID_WRITE,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pService->start();
    NimBLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    NimBLEDevice::startAdvertising();
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::startAdvertising();

    Serial.println("Characteristic defined! Now you can read it in your phone!");

    WiFiClass::mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
}

void loop() {
    // put your main code here, to run repeatedly:
    string val = pRead->getValue();
    pRead->setValue("");
    Serial.println(val.c_str());
    if (val[0] != 0) {
        Serial.println((string("|") + val + "|").c_str());


        if (val == "a") {
            vector<WiFiStorage> wifis;
            int n = WiFi.scanNetworks();

            for (int i = 0; i < n; i++) {
                wifis.emplace_back(WiFi.SSID(i).c_str(), WiFi.BSSID(i), WiFi.channel(i),
                                   WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
                delay(10);
            }
            Serial.write("About to send");
            send(wifis, pRead, pWrite);
        }
    }

    delay(1000);
}

