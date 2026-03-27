#pragma once
#include "config.h"

namespace GPIO {

    // --- 1. ПЕРЕХВАТЧИКИ ПОРТОВ (Безопасная проверка наличия) ---
    #ifdef GPIOA_BASE
    #define GPIO_MISC_A(V) V(A, GPIOA_BASE, __HAL_RCC_GPIOA_CLK_ENABLE)
    #else
    #define GPIO_MISC_A(V)
    #endif

    #ifdef GPIOB_BASE
    #define GPIO_MISC_B(V) V(B, GPIOB_BASE, __HAL_RCC_GPIOB_CLK_ENABLE)
    #else
    #define GPIO_MISC_B(V)
    #endif

    #ifdef GPIOC_BASE
    #define GPIO_MISC_C(V) V(C, GPIOC_BASE, __HAL_RCC_GPIOC_CLK_ENABLE)
    #else
    #define GPIO_MISC_C(V)
    #endif

    #ifdef GPIOD_BASE
    #define GPIO_MISC_D(V) V(D, GPIOD_BASE, __HAL_RCC_GPIOD_CLK_ENABLE)
    #else
    #define GPIO_MISC_D(V)
    #endif

    #ifdef GPIOE_BASE
    #define GPIO_MISC_E(V) V(E, GPIOE_BASE, __HAL_RCC_GPIOE_CLK_ENABLE)
    #else
    #define GPIO_MISC_E(V)
    #endif

    #ifdef GPIOF_BASE
    #define GPIO_MISC_F(V) V(F, GPIOF_BASE, __HAL_RCC_GPIOF_CLK_ENABLE)
    #else
    #define GPIO_MISC_F(V)
    #endif

    #ifdef GPIOG_BASE
    #define GPIO_MISC_G(V) V(G, GPIOG_BASE, __HAL_RCC_GPIOG_CLK_ENABLE)
    #else
    #define GPIO_MISC_G(V)
    #endif

    #ifdef GPIOH_BASE
    #define GPIO_MISC_H(V) V(H, GPIOH_BASE, __HAL_RCC_GPIOH_CLK_ENABLE)
    #else
    #define GPIO_MISC_H(V)
    #endif

    #ifdef GPIOI_BASE
    #define GPIO_MISC_I(V) V(I, GPIOI_BASE, __HAL_RCC_GPIOI_CLK_ENABLE)
    #else
    #define GPIO_MISC_I(V)
    #endif

    // Единый список портов
    #define GPIO_PORT_LIST(V) \
        GPIO_MISC_A(V) GPIO_MISC_B(V) GPIO_MISC_C(V) GPIO_MISC_D(V) \
        GPIO_MISC_E(V) GPIO_MISC_F(V) GPIO_MISC_G(V) GPIO_MISC_H(V) GPIO_MISC_I(V)

    // --- 2. ПАРАМЕТРЫ КОНФИГУРАЦИИ ---
    enum class AF : uint32_t { 
        AF0 = 0, AF1, AF2, AF3, AF4, AF5, AF6, AF7, 
        AF8, AF9, AF10, AF11, AF12, AF13, AF14, AF15 
    };

    enum class Mode : uint32_t { 
        Input    = GPIO_MODE_INPUT, 
        Output   = GPIO_MODE_OUTPUT_PP, 
        OutputOD = GPIO_MODE_OUTPUT_OD 
    };

    enum class ModeAlt : uint32_t { 
        PP     = GPIO_MODE_AF_PP, 
        OD     = GPIO_MODE_AF_OD, 
        Analog = GPIO_MODE_ANALOG 
    };

    enum class ModeIt : uint32_t { 
        Rising  = GPIO_MODE_IT_RISING, 
        Falling = GPIO_MODE_IT_FALLING, 
        Both    = GPIO_MODE_IT_RISING_FALLING 
    };

    enum class Pull  : uint32_t { None = GPIO_NOPULL, Up = GPIO_PULLUP, Down = GPIO_PULLDOWN };
    enum class Speed : uint32_t { Low = 0, Medium = 1, High = 2, VeryHigh = 3 };

    // --- 3. АВТО-ГЕНЕРАЦИЯ ENUM ID ---
    enum class ID : uintptr_t {
        #define M_ENUM(NAME, ADDR, CLK) NAME = ADDR,
        GPIO_PORT_LIST(M_ENUM)
        #undef M_ENUM
    };

    // --- 4. КЛАСС PIN ---
    class Pin {
    public:
        const ID       port_id;
        const uint16_t mask;
        const bool     inverted;

        constexpr Pin(ID id, uint8_t n, bool inv = false) 
            : port_id(id), mask(static_cast<uint16_t>(1U << n)), inverted(inv) {}

        inline GPIO_TypeDef* port() const { 
            return reinterpret_cast<GPIO_TypeDef*>(static_cast<uintptr_t>(port_id)); 
        }

        // Инициализация стандартного IO
        void Init(Mode mode, Pull pull = Pull::None, Speed speed = Speed::Low) const {
            EnableClock();
            GPIO_InitTypeDef cfg = {mask, (uint32_t)mode, (uint32_t)pull, (uint32_t)speed, 0};
            HAL_GPIO_Init(port(), &cfg);
        }

        // Инициализация периферии
        void Init(ModeAlt mode, AF af = AF::AF0, Pull pull = Pull::None, Speed speed = Speed::Low) const {
            EnableClock();
            GPIO_InitTypeDef cfg = {mask, (uint32_t)mode, (uint32_t)pull, (uint32_t)speed, (uint32_t)af};
            HAL_GPIO_Init(port(), &cfg);
        }

        // Инициализация прерываний
        void Init(ModeIt mode, Pull pull = Pull::None) const {
            EnableClock();
            GPIO_InitTypeDef cfg = {mask, (uint32_t)mode, (uint32_t)pull, 0, 0};
            HAL_GPIO_Init(port(), &cfg);
        }

        inline void Write(bool level) const {
            bool phys = inverted ? !level : level;
            port()->BSRR = phys ? (uint32_t)mask : (uint32_t)mask << 16;
        }

        inline void Set()    const { Write(true); }
        inline void Clear()  const { Write(false); }
        inline void Toggle() const { port()->ODR ^= mask; }
        inline bool Read()   const { return (port()->IDR & mask) != 0; }

    private:
        /**
         * @brief Скрытый метод включения тактирования. 
         * Вызывается автоматически внутри методов Init.
         */
        void EnableClock() const {
            switch (port_id) {
                #define M_CLOCK(NAME, ADDR, CLK) case ID::NAME: CLK(); break;
                GPIO_PORT_LIST(M_CLOCK)
                #undef M_CLOCK
            }
        }
    };

    // --- 5. ШАБЛОН ПОРТА И USING ---
    template<ID PortAddr>
    struct Port {
        template<uint8_t N> static constexpr Pin pin     = Pin(PortAddr, N);
        template<uint8_t N> static constexpr Pin pin_inv = Pin(PortAddr, N, true);
    };

    #define M_USING(NAME, ADDR, CLK) using Port##NAME = Port<ID::NAME>;
    GPIO_PORT_LIST(M_USING)
    #undef M_USING

} // namespace GPIO
