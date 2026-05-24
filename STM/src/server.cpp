#include "server.h"
#include <errno.h>

CServerBoard board;

namespace {

bool serial1TxHealthy() noexcept
{
    switch (board.serial1.getStatus()) {
    case BIF::IByteStream::Status::OK:
    case BIF::IByteStream::Status::OverFlowRX:
    case BIF::IByteStream::Status::OverFlowTX:
        return board.serial1.isOpen();
    default:
        return false;
    }
}

} // namespace

extern "C" int _write(int file, char *ptr, int len) {
    (void)file;

    if (len <= 0)
        return 0;
    if (!ptr) {
        errno = EINVAL;
        return -1;
    }
    if (!board.serial1.isOpen()) {
        errno = EIO;
        return -1;
    }

    int total = 0;
    while (total < len) {
        if (!serial1TxHealthy())
            break;

        const size_t chunk = static_cast<size_t>(len - total);
        const size_t n = board.serial1.write(reinterpret_cast<const uint8_t*>(ptr + total), chunk);
        if (n > 0) {
            total += static_cast<int>(n);
            continue;
        }

        while (serial1TxHealthy() && board.serial1.availableForWrite() == 0) {
        }
    }

    if (total == len)
        return total;

    if (total > 0)
        return total;

    errno = EIO;
    return -1;
}
