#pragma once

/**
 * PHL: идентификаторы периферии, IBase, IRQ::Vector (VTOR RAM через vector_table_ram).
 * Базовый слой железа — phl_base (defs, irq); GPIO, REG, VTOR RAM — core/.
 */
#include "config.h"
#include "core/gpio.h"
#include "core/phl_defs.hpp"
#include "core/irq.hpp"

namespace PHL {

namespace detail {

/**
 * Частота шины периферии (PCLK1/PCLK2) по PHL::ID — F4:
 * APB2: USART1/6, SPI1, SDIO; остальное из списка — APB1 (в т.ч. CAN, I2C).
 */
template <ID Id>
inline uint32_t input_clock_hz() noexcept
{
    constexpr uintptr_t base = static_cast<uintptr_t>(Id);
    constexpr Type type = GetType<Id>::value;

    if constexpr (type == Type::UART) {
#ifdef USART1_BASE
        if (base == USART1_BASE)
            return HAL_RCC_GetPCLK2Freq();
#endif
#ifdef USART6_BASE
        if (base == USART6_BASE)
            return HAL_RCC_GetPCLK2Freq();
#endif
        return HAL_RCC_GetPCLK1Freq();
    } else if constexpr (type == Type::SPI) {
#ifdef SPI1_BASE
        if (base == SPI1_BASE)
            return HAL_RCC_GetPCLK2Freq();
#endif
        return HAL_RCC_GetPCLK1Freq();
    } else if constexpr (type == Type::SDIO) {
        return HAL_RCC_GetPCLK2Freq();
    } else {
        // CAN, I2C, …
        return HAL_RCC_GetPCLK1Freq();
    }
}

} // namespace detail

    template <ID _id>
    class IBase;

#define M_IBASE_SINGLE_IRQ(NAME, TYPE_ENUM, A_, ORIG) \
    template<> \
    class IBase<ID::NAME> { \
    public: \
        static constexpr ID id = ID::NAME; \
        static constexpr Type type = Type::TYPE_ENUM; \
        static constexpr GPIO::AF af = GPIO::AF::AF##A_; \
        static constexpr Index index = Index::NAME; \
        static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_IRQn> IRQ{}; \
        static void EnableClock() { __HAL_RCC_##ORIG##_CLK_ENABLE(); } \
        static void DisableClock() { __HAL_RCC_##ORIG##_CLK_DISABLE(); } \
        static uint32_t InputClockHz() { return ::PHL::detail::input_clock_hz<ID::NAME>(); } \
    };

#define M_IBASE_UART(NAME, T_, A_, ORIG)  M_IBASE_SINGLE_IRQ(NAME, UART, A_, ORIG)
#define M_IBASE_SPI(NAME, T_, A_, ORIG)   M_IBASE_SINGLE_IRQ(NAME, SPI, A_, ORIG)
#define M_IBASE_SDIO(NAME, T_, A_, ORIG)  M_IBASE_SINGLE_IRQ(NAME, SDIO, A_, ORIG)

#define M_IBASE_I2C(NAME, T_, A_, ORIG) \
    template<> \
    class IBase<ID::NAME> { \
    public: \
        static constexpr ID id = ID::NAME; \
        static constexpr Type type = Type::I2C; \
        static constexpr GPIO::AF af = GPIO::AF::AF##A_; \
        static constexpr Index index = Index::NAME; \
        struct IRQ { \
            static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_EV_IRQn> Event{}; \
            static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_ER_IRQn> Error{}; \
        }; \
        static void EnableClock() { __HAL_RCC_##ORIG##_CLK_ENABLE(); } \
        static void DisableClock() { __HAL_RCC_##ORIG##_CLK_DISABLE(); } \
        static uint32_t InputClockHz() { return ::PHL::detail::input_clock_hz<ID::NAME>(); } \
    };

#define M_IBASE_CAN(NAME, T_, A_, ORIG) \
    template<> \
    class IBase<ID::NAME> { \
    public: \
        static constexpr ID id = ID::NAME; \
        static constexpr Type type = Type::CAN; \
        static constexpr GPIO::AF af = GPIO::AF::AF##A_; \
        static constexpr Index index = Index::NAME; \
        struct IRQ { \
            static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_TX_IRQn> TX{}; \
            static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_RX0_IRQn> RX0{}; \
            static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_RX1_IRQn> RX1{}; \
            static inline constexpr ::PHL::IRQ::Vector<ID::NAME, ORIG##_SCE_IRQn> SCE{}; \
        }; \
        static void EnableClock() { __HAL_RCC_##ORIG##_CLK_ENABLE(); } \
        static void DisableClock() { __HAL_RCC_##ORIG##_CLK_DISABLE(); } \
        static uint32_t InputClockHz() { return ::PHL::detail::input_clock_hz<ID::NAME>(); } \
    };

#define M_IBASE_DISPATCH(NAME, T_, A_, ORIG) M_IBASE_##T_(NAME, T_, A_, ORIG)

    PHL_PERIPH_LIST(M_IBASE_DISPATCH)

#undef M_IBASE_SINGLE_IRQ
#undef M_IBASE_UART
#undef M_IBASE_SPI
#undef M_IBASE_SDIO
#undef M_IBASE_I2C
#undef M_IBASE_CAN
#undef M_IBASE_DISPATCH



} // namespace PHL
