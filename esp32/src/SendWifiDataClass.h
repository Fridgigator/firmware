#ifndef ESP32_SENDWIFIDATACLASS_H
#define ESP32_SENDWIFIDATACLASS_H

#include "WiFiStorage.h"
#include <deque>
#include <optional>

class SendWifiDataClass {
  deque<uint8_t> returnData;
 public:
  explicit SendWifiDataClass(vector<WiFiStorage> &wifi);

  [[nodiscard]] vector<uint8_t> getSize() const;

  [[nodiscard]] optional<vector<uint8_t>> getData(int amnt);
  bool isEmpty();
};

#endif //ESP32_SENDWIFIDATACLASS_H
