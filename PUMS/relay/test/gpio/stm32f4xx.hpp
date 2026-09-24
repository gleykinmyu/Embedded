#ifndef GPIO_STM32F4XX_HPP
#define GPIO_STM32F4XX_HPP

/**
 * Семейство STM32F4xx. stm32f4xx.h открывает файл кристалла.
 * GPIOA — уже указатель; макроса нет, если порта на чипе нет.
 */
#include <stm32f4xx.h>

namespace GPIO {

using Reg = uint32_t;

namespace detail {

inline GPIO_TypeDef* const kPort[] = {
#if defined(GPIOA)
    GPIOA,
#endif
#if defined(GPIOB)
    GPIOB,
#endif
#if defined(GPIOC)
    GPIOC,
#endif
#if defined(GPIOD)
    GPIOD,
#endif
#if defined(GPIOE)
    GPIOE,
#endif
#if defined(GPIOF)
    GPIOF,
#endif
#if defined(GPIOG)
    GPIOG,
#endif
#if defined(GPIOH)
    GPIOH,
#endif
#if defined(GPIOI)
    GPIOI,
#endif
#if defined(GPIOJ)
    GPIOJ,
#endif
#if defined(GPIOK)
    GPIOK,
#endif
};

} // namespace detail

} // namespace GPIO

#endif

#ifdef GPIO_DECLARE_PORTS
namespace GPIO {

#define GPIO_N0 0
#if defined(GPIOA)
template <uint8_t Bit> struct PinAt<GPIO_N0, Bit> { using type = PinIrq; };
using PortA = Port<GPIO_N0, 16>;
#define GPIO_N1 (GPIO_N0 + 1)
#else
#define GPIO_N1 GPIO_N0
#endif

#if defined(GPIOB)
template <uint8_t Bit> struct PinAt<GPIO_N1, Bit> { using type = PinIrq; };
using PortB = Port<GPIO_N1, 16>;
#define GPIO_N2 (GPIO_N1 + 1)
#else
#define GPIO_N2 GPIO_N1
#endif

#if defined(GPIOC)
template <uint8_t Bit> struct PinAt<GPIO_N2, Bit> { using type = PinIrq; };
using PortC = Port<GPIO_N2, 16>;
#define GPIO_N3 (GPIO_N2 + 1)
#else
#define GPIO_N3 GPIO_N2
#endif

#if defined(GPIOD)
template <uint8_t Bit> struct PinAt<GPIO_N3, Bit> { using type = PinIrq; };
using PortD = Port<GPIO_N3, 16>;
#define GPIO_N4 (GPIO_N3 + 1)
#else
#define GPIO_N4 GPIO_N3
#endif

#if defined(GPIOE)
template <uint8_t Bit> struct PinAt<GPIO_N4, Bit> { using type = PinIrq; };
using PortE = Port<GPIO_N4, 16>;
#define GPIO_N5 (GPIO_N4 + 1)
#else
#define GPIO_N5 GPIO_N4
#endif

#if defined(GPIOF)
template <uint8_t Bit> struct PinAt<GPIO_N5, Bit> { using type = PinIrq; };
using PortF = Port<GPIO_N5, 16>;
#define GPIO_N6 (GPIO_N5 + 1)
#else
#define GPIO_N6 GPIO_N5
#endif

#if defined(GPIOG)
template <uint8_t Bit> struct PinAt<GPIO_N6, Bit> { using type = PinIrq; };
using PortG = Port<GPIO_N6, 16>;
#define GPIO_N7 (GPIO_N6 + 1)
#else
#define GPIO_N7 GPIO_N6
#endif

#if defined(GPIOH)
template <uint8_t Bit> struct PinAt<GPIO_N7, Bit> { using type = PinIrq; };
using PortH = Port<GPIO_N7, 16>;
#define GPIO_N8 (GPIO_N7 + 1)
#else
#define GPIO_N8 GPIO_N7
#endif

#if defined(GPIOI)
template <uint8_t Bit> struct PinAt<GPIO_N8, Bit> { using type = PinIrq; };
using PortI = Port<GPIO_N8, 16>;
#define GPIO_N9 (GPIO_N8 + 1)
#else
#define GPIO_N9 GPIO_N8
#endif

#if defined(GPIOJ)
template <uint8_t Bit> struct PinAt<GPIO_N9, Bit> { using type = PinIrq; };
using PortJ = Port<GPIO_N9, 16>;
#define GPIO_N10 (GPIO_N9 + 1)
#else
#define GPIO_N10 GPIO_N9
#endif

#if defined(GPIOK)
template <uint8_t Bit> struct PinAt<GPIO_N10, Bit> { using type = PinIrq; };
using PortK = Port<GPIO_N10, 16>;
#endif

#undef GPIO_N0
#undef GPIO_N1
#undef GPIO_N2
#undef GPIO_N3
#undef GPIO_N4
#undef GPIO_N5
#undef GPIO_N6
#undef GPIO_N7
#undef GPIO_N8
#undef GPIO_N9
#undef GPIO_N10

} // namespace GPIO
#endif
