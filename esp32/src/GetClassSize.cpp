#include <cstring>
#include "GetClassSize.h"

using namespace std;

GetSizeClass::GetSizeClass(array<uint8_t, 4> data) {
    size = 0;
    std::memcpy(&size, data.data(), sizeof(std::uint32_t));
}

[[nodiscard]] uint32_t GetSizeClass::getSize() const {
    return size;
}
