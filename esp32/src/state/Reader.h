#ifndef ESP32_SRC_STATE_READER_H_
#define ESP32_SRC_STATE_READER_H_
#include <variant>
#include <deque>

#include "NimBLEDevice.h"
class Reader {
  NimBLEAttValue *data;
 public:
  explicit Reader(NimBLEAttValue *ch);
  explicit Reader(std::deque<uint8_t> &val);
  std::deque<uint8_t> read();
};

#endif //ESP32_SRC_STATE_READER_H_
