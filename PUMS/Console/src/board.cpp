#include "board.hpp"
#include <errno.h>

CBoard board;

void CBoard::tick() noexcept
{
    if (!_ledAlive) {
        return;
    }

    const uint32_t now = GetTick();
    if ((now - _ledBlinkMs) >= 1000u) {
        _ledBlinkMs = now;
        led.Toggle();
    }
}

void CBoard::setLedAlive(bool alive) noexcept
{
    _ledAlive = alive;
    if (!alive) {
        led.Off();
    }
}

uint32_t boardClockMs() noexcept
{
    return board.GetTick();
}

namespace {

bool g_serial1_log_enabled = true;

bool serial1TxHealthy() noexcept
{
    if (!board.serial1.isOpen())
        return false;
    switch (board.serial1.getStatus()) {
    case BIF::IByteStream::Status::OK:
    case BIF::IByteStream::Status::OverFlowRX:
    case BIF::IByteStream::Status::DataError:
        return true;
    default:
        return false;
    }
}

} // namespace

void setSerial1LogEnabled(bool enabled) noexcept
{
    g_serial1_log_enabled = enabled;
}

extern "C" int _write(int file, char *ptr, int len) {
    (void)file;

    if (len <= 0)
        return 0;
    if (!ptr) {
        errno = EINVAL;
        return -1;
    }
    if (!g_serial1_log_enabled)
        return len;
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
            board.tick();
        }
    }

    if (total == len)
        return total;

    if (total > 0)
        return total;

    errno = EIO;
    return -1;
}
