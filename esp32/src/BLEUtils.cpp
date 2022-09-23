#include "BLEUtils.h"
NimBLEScanResults getScanResults() {
  auto scan = NimBLEDevice::getScan();
  scan->clearResults();
  delay(100);
  scan->start(5);
  delay(100);
  return scan->getResults();
}