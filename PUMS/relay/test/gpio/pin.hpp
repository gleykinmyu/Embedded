#pragma once

/**
 * Макет GPIO::Pin. Камень выбирает gpio_target.hpp.
 * В сборку попадает один файл ветки, он открывает настоящий заголовок регистров.
 */
#include <stdint.h>
#include "gpio_target.hpp"

namespace GPIO {

enum class Mode : uint8_t { Input, Output };

class Pin {
public:
    constexpr Pin(uint8_t port, uint8_t bit, bool inverted = false) noexcept
        : _port(port)
        , _mask(static_cast<Reg>(Reg{1} << bit))
        , _inverted(inverted)
    {
    }

    void Init(Mode mode) const noexcept;
    void Set() const noexcept;
    void Clear() const noexcept;
    void Write(bool level) const noexcept;
    [[nodiscard]] bool Read() const noexcept;

    void Toggle() const noexcept { Write(!Read()); }

private:
    const uint8_t _port;
    const Reg _mask;
    const bool _inverted;
};

template <uint8_t PortIndex, uint8_t Bit>
struct PinAt {
    using type = Pin;
};

template <uint8_t PortIndex, uint8_t PinCount, uint8_t Bit>
constexpr typename PinAt<PortIndex, Bit>::type make_pin() noexcept
{
    static_assert(Bit < PinCount, "GPIO: нет такого пина");
    return typename PinAt<PortIndex, Bit>::type{PortIndex, Bit};
}

template <uint8_t PortIndex, uint8_t PinCount>
struct Port {
    template <uint8_t N>
    static constexpr typename PinAt<PortIndex, N>::type pin =
        make_pin<PortIndex, PinCount, N>();
};

#include "gpio_pin.inl"

} // namespace GPIO
