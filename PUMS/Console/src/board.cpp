#include "board.hpp"

#include <cstddef>
#include <errno.h>

#include "ff.h"

CBoard board;

extern "C" DWORD get_fattime(void)
{
    return board.rtc.fatTime();
}


bool CBoard::tick() noexcept
{
    watchdog.kick();

    if (!_ledAlive) {
        return false;
    }

    const uint32_t now = GetTick();
    if ((now - _ledBlinkMs) < 1000u) {
        return false;
    }
    _ledBlinkMs = now;
    led.Toggle();
    return true;
}

void CBoard::setLedAlive(bool alive) noexcept
{
    _ledAlive = alive;
    if (!alive) {
        led.Off();
    }
}

bool CBoard::initCan(uint32_t bitrate) noexcept
{
    can.InitPins(GPIO::PortD::pin<0>, GPIO::PortD::pin<1>);
    return can.open(bitrate);
}

uint32_t boardClockMs() noexcept
{
    return board.GetTick();
}

namespace {

bool g_serial1_log_enabled = true;
Serial1LogSink g_serial1_log_sink = nullptr;

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

void setSerial1LogSink(Serial1LogSink sink) noexcept
{
    g_serial1_log_sink = sink;
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
    if (g_serial1_log_sink != nullptr)
        g_serial1_log_sink(ptr, static_cast<std::size_t>(len));
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
            /* Пустое ожидание: без kick IWDG и без LED (printf не должен «оживлять» плату). */
        }
    }

    if (total == len)
        return total;

    if (total > 0)
        return total;

    errno = EIO;
    return -1;
}
