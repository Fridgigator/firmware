#ifndef ESP32_GETWIFISTATESIZECLASS_H
#define ESP32_GETWIFISTATESIZECLASS_H

#include <cstdint>
#include <deque>
#include <vector>
#include <optional>
#include "WiFiState.h"

using namespace std;
class GetWiFiStateSizeClass {
  deque<uint8_t> returnData;
 public:
  explicit GetWiFiStateSizeClass(WiFiState wifi);

  [[nodiscard]] vector<uint8_t> getSize() const;

  [[nodiscard]] optional<vector<uint8_t>> getData(int amnt);

  bool isEmpty() const;
};

#endif //ESP32_GETWIFISTATESIZECLASS_H
