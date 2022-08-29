#ifndef ESP32_GETSIZECLASS_H
#define ESP32_GETSIZECLASS_H

#include <Arduino.h>
#include <cstdint>
#include <array>
#include "StateType.h"

using namespace std;

class GetSizeClass {
  uint32_t size = 0;
 public:

  explicit GetSizeClass(array<uint8_t, 4> data);
  [[nodiscard]] uint32_t getSize() const;
};

#endif //ESP32_GETSIZECLASS_H
