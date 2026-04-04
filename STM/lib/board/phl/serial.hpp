#pragma once

#include "../../Interfaces/iserial.hpp"
#include "core/gpio.h"
#include "core/irq.h"
#include "core/phl.h"

#include <stm32f4xx.h>

namespace PHL {

namespace detail {

inline USART_TypeDef* usart_ptr(PHL::ID id)
{
    return reinterpret_cast<USART_TypeDef*>(static_cast<uintptr_t>(id));
}

inline uint32_t uart_pclk_hz(PHL::ID id)
{
    switch (static_cast<uintptr_t>(id)) {
#if defined(USART1_BASE)
        case USART1_BASE:
#endif
#if defined(USART6_BASE)
        case USART6_BASE:
#endif
#if defined(USART1_BASE) || defined(USART6_BASE)
            return HAL_RCC_GetPCLK2Freq();
#endif
#if defined(USART2_BASE)
        case USART2_BASE:
#endif
#if defined(USART3_BASE)
        case USART3_BASE:
#endif
#if defined(UART4_BASE)
        case UART4_BASE:
#endif
#if defined(UART5_BASE)
        case UART5_BASE:
#endif
#if defined(USART2_BASE) || defined(USART3_BASE) || defined(UART4_BASE) || defined(UART5_BASE)
            return HAL_RCC_GetPCLK1Freq();
#endif
        default:
            return HAL_RCC_GetPCLK1Freq();
    }
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

/// BRR при OVERS16: USARTDIV = f_ck / (16 × baud), поля mantissa[15:4] + fraction[3:0].
inline uint16_t uart_brr(uint32_t pclk_hz, uint32_t baud)
{
    if (baud == 0)
        return 0;
    const uint32_t divisor = baud * 16U;
    uint32_t mant = pclk_hz / divisor;
    const uint32_t rem = pclk_hz % divisor;
    uint32_t frac = (rem * 16U + divisor / 2U) / divisor;
    if (frac > 15U) {
        mant++;
        frac = 0;
    }
    return static_cast<uint16_t>((mant << 4) | (frac & 15U));
}

} // namespace detail

/**
 * Реализация BIF::ISerial для USART/UART из PHL::ID.
 * Регистры — через CMSIS (USART_TypeDef); тактирование — PHL::IBase::EnableClock();
 * прерывания — IRQ::IHandler + векторы из core/irq.cpp.
 *
 * Перед open() настройте выводы TX/RX (например InitPins).
 */
template <PHL::ID UartId, size_t TxSize = 256, size_t RxSize = 256>
class Serial : public BIF::ISerial<TxSize, RxSize>, public IRQ::IHandler<UartId>
{
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "Serial: только PHL::ID с Type::UART");

    using Dev = PHL::IBase<UartId>;

    USART_TypeDef* hw() const { return detail::usart_ptr(UartId); }

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
        USART_TypeDef* u = hw();

        u->CR1 &= ~USART_CR1_UE;
        u->CR1 = 0;
        u->CR2 = 0;
        u->CR3 = 0;

        const uint32_t pclk = detail::uart_pclk_hz(UartId);
        u->BRR = detail::uart_brr(pclk, baudrate);

        u->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

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
        u->CR1 &= ~(USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);

        this->_isOpen = false;
    }

    void IrqHandler() override
    {
        USART_TypeDef* u = hw();
        while ((u->SR & USART_SR_RXNE) != 0)
            this->IRQ_RX_Handler();
        if ((u->SR & USART_SR_TXE) != 0)
            this->IRQ_TX_Handler();
    }

protected:
    void IRQ_TX_Enable() override { hw()->CR1 |= USART_CR1_TXEIE; }

    void IRQ_TX_Disable() override { hw()->CR1 &= ~USART_CR1_TXEIE; }

    bool isHardwareTxBusy() const override { return (hw()->SR & USART_SR_TC) == 0; }

    uint8_t readHardware() override { return static_cast<uint8_t>(hw()->DR & 0xFFU); }

    void writeHardware(uint8_t data) override { hw()->DR = data; }

    bool checkErrors() override
    {
        const uint32_t sr = hw()->SR;
        if (sr & USART_SR_FE)
            this->_isFrameError = true;
        if (sr & USART_SR_NE)
            this->_isBitError = true;
        if (sr & USART_SR_ORE)
            this->_isDisconnected = true;
        return (sr & (USART_SR_FE | USART_SR_NE)) != 0;
    }

    void lock() override
    {
        _primaskSave = __get_PRIMASK();
        __disable_irq();
    }

    void unlock() override { __set_PRIMASK(_primaskSave); }
};

} // namespace PHL
