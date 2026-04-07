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
