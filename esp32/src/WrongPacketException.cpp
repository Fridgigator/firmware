#include "WrongPacketException.h"

const char *WrongPacketException::what() {
    return "Wrong packet";
}
