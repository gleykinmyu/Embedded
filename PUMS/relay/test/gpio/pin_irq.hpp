#pragma once

/**
 * Пин с линией прерывания. Наследует Pin, поэтому встаёт в const Pin&.
 * Обычный Pin в const PinIrq& не встаёт.
 */
#include "pin.hpp"

namespace GPIO {

enum class Edge : uint8_t { Rising, Falling, Both };

class PinIrq : public Pin {
public:
    using Pin::Pin;

    [[nodiscard]] bool attach(Edge edge, void (*handler)()) const noexcept;
    void detach() const noexcept;
};

inline bool PinIrq::attach(Edge, void (*handler)()) const noexcept
{
    return handler != nullptr;
}

inline void PinIrq::detach() const noexcept {}

} // namespace GPIO
