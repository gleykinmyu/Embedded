#pragma once

/**
 * @file serialConsole.hpp
 *
 * Разбор строк с debug-UART (serial monitor):
 * - пустая строка (Enter) — отдельное событие;
 * - `reboot` — программный сброс MCU (NVIC_SystemReset).
 *
 * Неблокирующий `poll()` + блокирующий `waitEnter()` с колбэком tick (например app.update()).
 */

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ibyte_stream.hpp"

#if defined(__cplusplus)
extern "C" {
#endif
#include <stm32f4xx.h>
#if defined(__cplusplus)
}
#endif

namespace dbg {

enum class ConsoleEvent : uint8_t {
    None,    /**< Строка ещё не завершена. */
    Enter,   /**< Пустая строка (только Enter). */
    Reboot,  /**< Команда reboot. */
    Unknown, /**< Непустая строка, команда не распознана. */
};

/** Программный сброс MCU (не возвращается). */
inline void softwareReboot() noexcept
{
    __DSB();
    __ISB();
    NVIC_SystemReset();
    for (;;) {
    }
}

class SerialConsole {
public:
    static constexpr std::size_t kMaxLine = 48u;

    explicit SerialConsole(BIF::IByteStream& serial) noexcept
        : _serial(serial)
    {
    }

    /** Текст последней завершённой строки (валиден до следующего poll/waitEnter). */
    [[nodiscard]] const char* line() const noexcept { return _line; }

    /**
     * Читает доступные байты из UART.
     * @p tick вызывается на каждой итерации (удобно крутить Nextion update()).
     */
    template<typename TickFn>
    ConsoleEvent poll(TickFn&& tick) noexcept
    {
        for (;;) {
            tick();

            if (_serial.available() == 0u)
                return ConsoleEvent::None;

            uint8_t byte = 0u;
            if (_serial.read(&byte, 1u) == 0u)
                return ConsoleEvent::None;

            if (byte == '\r' || byte == '\n') {
                if (byte == '\n' && _prevWasCr) {
                    _prevWasCr = false;
                    continue;
                }
                _prevWasCr = (byte == '\r');
                return finishLine();
            }

            _prevWasCr = false;

            if (_len >= (kMaxLine - 1u))
                continue;

            _line[_len++] = static_cast<char>(byte);
        }
    }

    /** Ждёт пустую строку; reboot и неизвестные команды обрабатываются по ходу ожидания. */
    template<typename TickFn>
    void waitEnter(TickFn&& tick) noexcept
    {
        for (;;) {
            switch (poll(tick)) {
            case ConsoleEvent::None:
                continue;
            case ConsoleEvent::Enter:
                return;
            case ConsoleEvent::Reboot:
                std::printf("[console] reboot\n");
                softwareReboot();
                break;
            case ConsoleEvent::Unknown:
                std::printf("[console] unknown: \"%s\" (Enter=next, reboot=reset)\n", _line);
                continue;
            }
        }
    }

private:
    static void trimInPlace(char* s, std::size_t& len) noexcept
    {
        while (len > 0u && std::isspace(static_cast<unsigned char>(s[len - 1u])) != 0)
            --len;
        std::size_t start = 0u;
        while (start < len && std::isspace(static_cast<unsigned char>(s[start])) != 0)
            ++start;
        if (start > 0u) {
            len -= start;
            std::memmove(s, s + start, len);
        }
        s[len] = '\0';
    }

    static bool iequals(const char* a, const char* b) noexcept
    {
        if (a == nullptr || b == nullptr)
            return a == b;

        while (*a != '\0' && *b != '\0') {
            const unsigned char ca = static_cast<unsigned char>(*a);
            const unsigned char cb = static_cast<unsigned char>(*b);
            if (std::tolower(ca) != std::tolower(cb))
                return false;
            ++a;
            ++b;
        }
        return *a == *b;
    }

    ConsoleEvent finishLine() noexcept
    {
        _line[_len] = '\0';
        trimInPlace(_line, _len);

        ConsoleEvent ev = ConsoleEvent::Unknown;
        if (_len == 0u)
            ev = ConsoleEvent::Enter;
        else if (iequals(_line, "reboot"))
            ev = ConsoleEvent::Reboot;

        _len = 0u;
        return ev;
    }

    BIF::IByteStream& _serial;
    char _line[kMaxLine]{};
    std::size_t _len = 0u;
    bool _prevWasCr = false;
};

} // namespace dbg
