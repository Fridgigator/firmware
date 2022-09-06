#include "GetCommandClass.h"

using namespace std;

GetCommandClass::GetCommandClass(uint32_t size) {
  this->totalLengthOfData = size;
}

optional<vector<uint8_t>> GetCommandClass::read(deque<uint8_t> &ch) {
  while (!ch.empty()) {
    uint8_t v = ch.front();
    ch.pop_front();
    data.push_back(v);
  }
  if (data.size() >= totalLengthOfData) {
    return {vector(data.begin(), data.end())};
  } else {
    return std::nullopt;
  }
}
