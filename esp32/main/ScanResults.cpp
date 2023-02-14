#include "ScanResults.h"
#include "NimBLEDevice.h"
#include "lib/ArduinoSupport/ArduinoSupport.h"

NimBLEScanResults ScanResults::getScanResults() {

    scanResults = BLEDevice::getScan();
    mutex.lock();
    scanResults->clearResults();
    auto returnVal = scanResults->getResults(5'000);
    mutex.unlock();
    return returnVal;
}

ScanResults::ScanResults() = default;

ScanResults::~ScanResults() {
    if (scanResults != nullptr) {
        scanResults->stop();
    }
}