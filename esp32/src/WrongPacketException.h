#ifndef ESP32_WRONGPACKETEXCEPTION_H
#define ESP32_WRONGPACKETEXCEPTION_H

#include <exception>

class WrongPacketException : public std::exception {
public:
    const char *what();

};


#endif //ESP32_WRONGPACKETEXCEPTION_H
