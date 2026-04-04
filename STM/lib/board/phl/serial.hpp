#pragma once

#include "../../Interfaces/iserial.hpp"
#include "core/gpio.h"
#include "core/irq.h"
#include "core/phl.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_usart.h"

namespace PHL {

namespace detail {

inline USART_TypeDef* usart_ptr(PHL::ID id)
{
    return reinterpret_cast<USART_TypeDef*>(static_cast<uintptr_t>(id));
}

inline IRQn_Type uart_irqn(PHL::ID id)
{
    switch (static_cast<uintptr_t>(id)) {
#ifdef USART1_BASE
        case USART1_BASE: return USART1_IRQn;
#endif
#ifdef USART2_BASE
        case USART2_BASE: return USART2_IRQn;
#endif
#ifdef USART3_BASE
        case USART3_BASE: return USART3_IRQn;
#endif
#ifdef UART4_BASE
        case UART4_BASE: return UART4_IRQn;
#endif
#ifdef UART5_BASE
        case UART5_BASE: return UART5_IRQn;
#endif
#ifdef USART6_BASE
        case USART6_BASE: return USART6_IRQn;
#endif
        default:
            return static_cast<IRQn_Type>(0);
    }
}

} // namespace detail

/**
 * Реализация BIF::ISerial для USART/UART из PHL::ID.
 * Инициализация — HAL (HAL_UART_Init); тактирование — PHL::IBase::EnableClock();
 * прерывания NVIC — HAL; горячий путь (IRQ, TX/RX) — LL.
 *
 * Перед open() настройте выводы TX/RX (например InitPins).
 */
template <PHL::ID UartId, size_t TxSize = 256, size_t RxSize = 256>
class Serial : public BIF::ISerial<TxSize, RxSize>, public IRQ::IHandler<UartId>
{
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "Serial: только PHL::ID с Type::UART");

    using Dev = PHL::IBase<UartId>;

    USART_TypeDef* hw() const { return detail::usart_ptr(UartId); }

    UART_HandleTypeDef _huart{};
    uint32_t _primaskSave = 0;

public:
    Serial() = default;

    /// TX/RX в режиме AF PP; подтяжка и скорость — как у типичного UART.
    void InitPins(const GPIO::Pin& tx, const GPIO::Pin& rx) const
    {
        tx.Init(GPIO::ModeAlt::PP, Dev::af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        rx.Init(GPIO::ModeAlt::PP, Dev::af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
    }

    bool open(uint32_t baudrate) override
    {
        if (baudrate == 0)
            return false;

        Dev::EnableClock();

        _huart.Instance = hw();
        _huart.Init.BaudRate = baudrate;
        _huart.Init.WordLength = UART_WORDLENGTH_8B;
        _huart.Init.StopBits = UART_STOPBITS_1;
        _huart.Init.Parity = UART_PARITY_NONE;
        _huart.Init.Mode = UART_MODE_TX_RX;
        _huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        _huart.Init.OverSampling = UART_OVERSAMPLING_16;

        if (HAL_UART_Init(&_huart) != HAL_OK)
            return false;

        LL_USART_EnableIT_RXNE(hw());

        const IRQn_Type irq = detail::uart_irqn(UartId);
        if (irq != static_cast<IRQn_Type>(0)) {
            HAL_NVIC_SetPriority(irq, 5, 0);
            HAL_NVIC_EnableIRQ(irq);
        }

        this->_isOpen = true;
        return true;
    }

    void close() override
    {
        const IRQn_Type irq = detail::uart_irqn(UartId);
        if (irq != static_cast<IRQn_Type>(0))
            HAL_NVIC_DisableIRQ(irq);

        USART_TypeDef* u = hw();
        LL_USART_DisableIT_RXNE(u);
        LL_USART_DisableIT_TXE(u);

        HAL_UART_DeInit(&_huart);

        this->_isOpen = false;
    }

    void IrqHandler() override
    {
        USART_TypeDef* u = hw();
        while (LL_USART_IsActiveFlag_RXNE(u))
            this->IRQ_RX_Handler();
        if (LL_USART_IsActiveFlag_TXE(u))
            this->IRQ_TX_Handler();
    }

protected:
    void IRQ_TX_Enable() override { LL_USART_EnableIT_TXE(hw()); }

    void IRQ_TX_Disable() override { LL_USART_DisableIT_TXE(hw()); }

    bool isHardwareTxBusy() const override { return LL_USART_IsActiveFlag_TC(hw()) == 0U; }

    uint8_t readHardware() override { return LL_USART_ReceiveData8(hw()); }

    void writeHardware(uint8_t data) override { LL_USART_TransmitData8(hw(), data); }

    bool checkErrors() override
    {
        const uint32_t sr = LL_USART_ReadReg(hw(), SR);
        if (sr & LL_USART_SR_FE)
            this->_isFrameError = true;
        if (sr & LL_USART_SR_NE)
            this->_isBitError = true;
        if (sr & LL_USART_SR_ORE)
            this->_isDisconnected = true;
        return (sr & (LL_USART_SR_FE | LL_USART_SR_NE)) != 0U;
    }

    void lock() override
    {
        _primaskSave = __get_PRIMASK();
        __disable_irq();
    }

    void unlock() override { __set_PRIMASK(_primaskSave); }
};

} // namespace PHL
