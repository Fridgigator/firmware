#include "BLEUtils.h"
NimBLEScanResults ScanResults::getScanResults() {
  scanResults = NimBLEDevice::getScan();
  scanResults->clearResults();
  delay(100);
  scanResults->start(5);
  delay(100);
  return scanResults->getResults();
}
ScanResults::ScanResults() = default;

ScanResults::~ScanResults(){
  if (scanResults != nullptr) {
    scanResults->stop();
    scanResults->clearResults();
  }
}