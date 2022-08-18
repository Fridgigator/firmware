#include "DecodeException.h"

const char *DecodeException::what() {
    return "wrong char after 1";
}
