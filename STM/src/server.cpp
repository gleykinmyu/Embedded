#include "server.h"

extern "C" int _write(int file, char *ptr, int len) {
    return board.serial1.write(reinterpret_cast<const uint8_t*>(ptr), len);
}