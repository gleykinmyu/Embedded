#pragma once

#include <stdint.h>

namespace BIF {

enum class PinMode : uint8_t { Input, Output };

enum class PinPull : uint8_t { None, Up };

/**
 * Ножка как периферия: реле, кнопка, разовый Init.
 * Горячий ногодрыг протокола сюда не ходит — его драйвер пишет регистр сам.
 */
class IDigitalPin {
public:
    virtual ~IDigitalPin() = default;

    virtual void Init(PinMode mode, PinPull pull = PinPull::None) = 0;
    virtual void Set() = 0;
    virtual void Clear() = 0;
    virtual void Toggle() = 0;
    [[nodiscard]] virtual bool Read() const = 0;
};

} // namespace BIF
