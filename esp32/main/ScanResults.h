#ifndef ESP32_SRC_BLEUTILS_H_
#define ESP32_SRC_BLEUTILS_H_

#include <mutex>
#include "NimBLEScan.h"

/// ScanResults gives NimBLEScan an RAII interface, ensuring that it stops scanning when a ScanResults
/// instance goes out of scope and to make it thread safe
class ScanResults {
    std::mutex mutex;
    NimBLEScan *scanResults = nullptr;
private:
public:
    ScanResults();

    NimBLEScanResults getScanResults();

    ~ScanResults();

};

#endif //ESP32_SRC_BLEUTILS_H_
