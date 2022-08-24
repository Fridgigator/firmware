#ifndef ESP32_GETCLASSSIZE_H
#define ESP32_GETCLASSSIZE_H

#include <Arduino.h>
#include <cstdint>
#include <array>

using namespace std;

class GetSizeClass {
    uint32_t size = 0;
public:
    GetSizeClass() = default;

    explicit GetSizeClass(array<uint8_t, 4> data);

    [[nodiscard]] uint32_t getSize() const;
};


#endif //ESP32_GETCLASSSIZE_H
