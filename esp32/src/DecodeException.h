#ifndef ESP32_DECODEEXCEPTION_H
#define ESP32_DECODEEXCEPTION_H

#include <exception>
#include <string>

using namespace std;

class DecodeException : public exception {
public:
    const char *what();
};


#endif //ESP32_DECODEEXCEPTION_H
