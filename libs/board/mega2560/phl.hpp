#pragma once

/**
 * Идентификаторы периферии ATmega2560 и слот обработчика.
 * Указатель handler определяется в isr.hpp (один .cpp): AVR не умеет VTOR, как Cortex-M.
 */
#include <stdint.h>

namespace PHL {

enum class Type : uint8_t { UART, SPI, TWI };

enum class ID : uint8_t {
    SERIAL0,
    SERIAL1,
    SERIAL2,
    SERIAL3,
    SPI0,
    TWI0,
};

template <ID id>
struct GetType;

template <> struct GetType<ID::SERIAL0> { static constexpr Type value = Type::UART; };
template <> struct GetType<ID::SERIAL1> { static constexpr Type value = Type::UART; };
template <> struct GetType<ID::SERIAL2> { static constexpr Type value = Type::UART; };
template <> struct GetType<ID::SERIAL3> { static constexpr Type value = Type::UART; };
template <> struct GetType<ID::SPI0> { static constexpr Type value = Type::SPI; };
template <> struct GetType<ID::TWI0> { static constexpr Type value = Type::TWI; };

template <ID Id>
struct Irq {
    static void (*handler)() noexcept;

    static void set(void (*fn)() noexcept) noexcept { handler = fn; }

    static void invoke() noexcept
    {
        if (handler != nullptr)
            handler();
    }
};

} // namespace PHL
