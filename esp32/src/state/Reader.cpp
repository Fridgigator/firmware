#include "Reader.h"
using namespace std;
#ifdef ARDUINO

Reader::Reader(NimBLEAttValue *ch) {
  this->data = ch;
}

deque<uint8_t> Reader::read() {
  deque<uint8_t> returnValue;
  for (int i = 0; i < data->length(); i++) {
    returnValue.emplace_back(data->data()[i]);
  }
  return returnValue;

}
#endif