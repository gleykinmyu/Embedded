#pragma once
#include "config.h"
#include "phl_misc.h"
#include "gpio.h"

namespace PHL {


    // --- 1. ИНДИВИДУАЛЬНЫЕ ПЕРЕХВАТЧИКИ (Проверка наличия и унификация имен) ---
    // Формат: V(Логическое_Имя, Тип, Номер_AF, Оригинальное_Имя_CMSIS)

    // UART (Унифицировано до PHL_MISC_UARTx)
    #ifdef USART1_BASE
    #define PHL_MISC_UART1(V) V(UART1, UART, 7, USART1)
    #undef USART1
    #else
    #define PHL_MISC_UART1(V)
    #endif

    #ifdef USART2_BASE
    #define PHL_MISC_UART2(V) V(UART2, UART, 7, USART2)
    #undef USART2
    #else
    #define PHL_MISC_UART2(V)
    #endif

    #ifdef USART3_BASE
    #define PHL_MISC_UART3(V) V(UART3, UART, 7, USART3)
    #undef USART3
    #else
    #define PHL_MISC_UART3(V)
    #endif

    #ifdef UART4_BASE
    #define PHL_MISC_UART4(V) V(UART4, UART, 8, UART4)
    #undef UART4
    #else
    #define PHL_MISC_UART4(V)
    #endif

    #ifdef UART5_BASE
    #define PHL_MISC_UART5(V) V(UART5, UART, 8, UART5)
    #undef UART5
    #else
    #define PHL_MISC_UART5(V)
    #endif

    #ifdef USART6_BASE
    #define PHL_MISC_UART6(V) V(UART6, UART, 8, USART6)
    #undef USART6
    #else
    #define PHL_MISC_UART6(V)
    #endif

    // SPI
    #ifdef SPI1_BASE
    #define PHL_MISC_SPI1(V) V(SPI1, SPI, 5, SPI1)
    #undef SPI1
    #else
    #define PHL_MISC_SPI1(V)
    #endif

    #ifdef SPI2_BASE
    #define PHL_MISC_SPI2(V) V(SPI2, SPI, 5, SPI2)
    #undef SPI2
    #else
    #define PHL_MISC_SPI2(V)
    #endif

    #ifdef SPI3_BASE
    #define PHL_MISC_SPI3(V) V(SPI3, SPI, 6, SPI3)
    #undef SPI3
    #else
    #define PHL_MISC_SPI3(V)
    #endif

    // I2C
    #ifdef I2C1_BASE
    #define PHL_MISC_I2C1(V) V(I2C1, I2C, 4, I2C1)
    #undef I2C1
    #else
    #define PHL_MISC_I2C1(V)
    #endif

    #ifdef I2C2_BASE
    #define PHL_MISC_I2C2(V) V(I2C2, I2C, 4, I2C2)
    #undef I2C2
    #else
    #define PHL_MISC_I2C2(V)
    #endif

    // CAN
    #ifdef CAN1_BASE
    #define PHL_MISC_CAN1(V) V(CAN1, CAN, 9, CAN1)
    #undef CAN1
    #else
    #define PHL_MISC_CAN1(V)
    #endif

    // SDIO / SDMMC
    #if defined(SDIO_BASE)
    #define PHL_MISC_SDIO(V) V(SDIO, SDIO, 12, SDIO)
    #undef SDIO
    #elif defined(SDMMC1_BASE)
    #define PHL_MISC_SDIO(V) V(SDIO, SDIO, 12, SDMMC1)
    #undef SDMMC1
    #else
    #define PHL_MISC_SDIO(V)
    #endif

    // --- 2. СПИСОК ТИПОВ ПЕРИФЕРИИ ---
    #define PHL_TYPE_LIST(V) V(UART) V(SPI) V(I2C) V(CAN) V(SDIO)

    enum class Type {
        #define M_GEN_TYPE(NAME) NAME,
        PHL_TYPE_LIST(M_GEN_TYPE)
        #undef M_GEN_TYPE
        Count 
    };

    // --- 3. ЕДИНЫЙ ДИНАМИЧЕСКИЙ СПИСОК МОДУЛЕЙ ---
    #define PHL_PERIPH_LIST(V) \
        PHL_MISC_UART1(V) PHL_MISC_UART2(V) PHL_MISC_UART3(V) \
        PHL_MISC_UART4(V) PHL_MISC_UART5(V) PHL_MISC_UART6(V) \
        PHL_MISC_SPI1(V)  PHL_MISC_SPI2(V)  PHL_MISC_SPI3(V)  \
        PHL_MISC_I2C1(V)  PHL_MISC_I2C2(V)  \
        PHL_MISC_CAN1(V)  PHL_MISC_SDIO(V)

    // --- 4. АВТО-ГЕНЕРАЦИЯ ---

    // Идентификаторы (базовые адреса) и авто-подсчет количества модулей
    enum class ID : uintptr_t {
        #define M_ENUM_ID(NAME, T_, A_, ORIG) NAME = ORIG##_BASE,
        PHL_PERIPH_LIST(M_ENUM_ID)
        #undef M_ENUM_ID

        #define M_COUNT(NAME, T_, A_, ORIG) +1
        Count = 0 PHL_PERIPH_LIST(M_COUNT)
        #undef M_COUNT
    };

    static constexpr size_t PeriphCount = static_cast<size_t>(ID::Count);

    // Плотные индексы (0, 1, 2...) для минимизации Registry
    enum class Index : uint8_t {
        #define M_ENUM_IDX(NAME, T_, A_, ORIG) NAME,
        PHL_PERIPH_LIST(M_ENUM_IDX)
        #undef M_ENUM_IDX
    };

    // Трейт для связи ID с Type
    template<ID id> struct GetType;
    #define M_GET_TYPE(NAME, T_, A_, ORIG) \
        template<> struct GetType<ID::NAME> { static constexpr Type value = Type::T_; };
    PHL_PERIPH_LIST(M_GET_TYPE)

    // Специализированный базовый класс IBase
    template<ID _id> class IBase;

    #define M_IBASE_SPEC(NAME, T_, A_, ORIG) \
    template<> class IBase<ID::NAME> { \
    public: \
        static constexpr ID       id    = ID::NAME; \
        static constexpr Type     type  = Type::T_; \
        static constexpr GPIO::AF af    = GPIO::AF::AF##A_; \
        static constexpr Index    index = Index::NAME; \
        static void EnableClock() { __HAL_RCC_##ORIG##_CLK_ENABLE(); } \
    };
    PHL_PERIPH_LIST(M_IBASE_SPEC)
    #undef M_IBASE_SPEC

} // namespace PHL
