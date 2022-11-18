#ifndef ESP32_SRC_BLEUTILS_H_
#define ESP32_SRC_BLEUTILS_H_
#include <Arduino.h>

#include "NimBLEDevice.h"
class ScanResults {
  NimBLEScan *scanResults = nullptr;
 private:
 public:
  ScanResults();

  NimBLEScanResults getScanResults();
  ~ScanResults();

};
#endif //ESP32_SRC_BLEUTILS_H_
