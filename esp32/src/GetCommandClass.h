#ifndef ESP32_GETCOMMANDCLASS_H
#define ESP32_GETCOMMANDCLASS_H

#include <cstdint>
#include <deque>
#include <vector>
#include <optional>

using namespace std;

class GetCommandClass {
  deque<uint8_t> data;
  uint32_t totalLengthOfData;
 public:
  explicit GetCommandClass(uint32_t size);

  [[nodiscard]]
  optional<vector<uint8_t>> read(deque<uint8_t> &ch);
};

#endif //ESP32_GETCOMMANDCLASS_H
