#ifndef GPIO_ESP32_HPP
#define GPIO_ESP32_HPP

/** ESP32: out — GPIO0..31, out1 — 8 бит, GPIO32..39. */
#define GPIO GPIO_HW
#include "soc/gpio_struct.h"
#undef GPIO

namespace GPIO {

using Reg = uint32_t;

namespace detail {

inline volatile uint32_t* const out[2] = { &GPIO_HW.out, &GPIO_HW.out1.val };
inline volatile uint32_t* const in[2] = { &GPIO_HW.in, &GPIO_HW.in1.val };

} // namespace detail

} // namespace GPIO

#endif

#ifdef GPIO_DECLARE_PORTS
namespace GPIO {

// Банк 0, GPIO0..31. Прерывание есть у каждой ножки банка.
template <uint8_t Bit> struct PinAt<0, Bit> { using type = PinIrq; };

using Bank0 = Port<0, 32>;

} // namespace GPIO
#endif
