#pragma once
#include "stm32f4xx_hal.h"

#include "../../Interfaces/iserial.hpp"
#include "core/gpio.h"
#include "core/irq.h"
#include "core/phl.h"


namespace PHL {

namespace detail {

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
 * прерывания NVIC и биты TXE/RXNE — HAL (__HAL_UART_*), без LL (совместимость с HAL_UART_Init).
 *
 * Перед open() настройте выводы TX/RX (например InitPins).
 */
template <PHL::ID UartId, size_t TxSize = 256, size_t RxSize = 256>
class Serial : private PHL::IBase<UartId>, public IRQ::IHandler<UartId>, public BIF::ISerial<TxSize, RxSize>
{
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "Serial: только PHL::ID с Type::UART");

    USART_TypeDef* hw() const { return reinterpret_cast<USART_TypeDef*>(static_cast<uintptr_t>(UartId)); }

    UART_HandleTypeDef _huart{};
    uint32_t _primaskSave = 0;

public:
    Serial() = default;

    /// TX/RX в режиме AF PP; подтяжка и скорость — как у типичного UART.
    void InitPins(const GPIO::Pin& tx, const GPIO::Pin& rx) const
    {
        tx.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        rx.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
    }

    bool open(uint32_t baudrate) override
    {
        if (baudrate == 0)
            return false;

        this->EnableClock();

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

        __HAL_UART_ENABLE_IT(&_huart, UART_IT_RXNE);

        const IRQn_Type irq = detail::uart_irqn(UartId);
        HAL_NVIC_SetPriority(irq, 5, 0);
        HAL_NVIC_EnableIRQ(irq);

        this->_isOpen = true;
        return true;
    }

    void close() override
    {
        const IRQn_Type irq = detail::uart_irqn(UartId);
        HAL_NVIC_DisableIRQ(irq);

        __HAL_UART_DISABLE_IT(&_huart, UART_IT_RXNE);
        __HAL_UART_DISABLE_IT(&_huart, UART_IT_TXE);

        HAL_UART_DeInit(&_huart);

        this->_isOpen = false;
    }

    void IrqHandler() override
    {
        // Как в HAL_UART_IRQHandler: реагируем только если и флаг в SR, и соответствующий IE в CR1.
        while (__HAL_UART_GET_FLAG(&_huart, UART_FLAG_RXNE) &&
               __HAL_UART_GET_IT_SOURCE(&_huart, UART_IT_RXNE)) {
            this->IRQ_RX_Handler();
        }
        if (__HAL_UART_GET_FLAG(&_huart, UART_FLAG_TXE) &&
            __HAL_UART_GET_IT_SOURCE(&_huart, UART_IT_TXE)) {
            this->IRQ_TX_Handler();
        }
    }

protected:
    void IRQ_TX_Enable() override { __HAL_UART_ENABLE_IT(&_huart, UART_IT_TXE); }

    void IRQ_TX_Disable() override { __HAL_UART_DISABLE_IT(&_huart, UART_IT_TXE); }

    bool isHardwareTxBusy() const override
    {
        return (__HAL_UART_GET_FLAG(&_huart, UART_FLAG_TC) == RESET);
    }

    uint8_t readHardware() override
    {
        return static_cast<uint8_t>(READ_REG(hw()->DR) & 0xFFU);
    }

    void writeHardware(uint8_t data) override { WRITE_REG(hw()->DR, data); }

    bool checkErrors() override
    {
        const uint32_t sr = READ_REG(hw()->SR);
        if ((sr & USART_SR_FE) != 0U)
            this->_isFrameError = true;
        if ((sr & USART_SR_NE) != 0U)
            this->_isBitError = true;
        if ((sr & USART_SR_ORE) != 0U)
            this->_isDisconnected = true;
        return (sr & (USART_SR_FE | USART_SR_NE)) != 0U;
    }

    void lock() override
    {
        _primaskSave = __get_PRIMASK();
        __disable_irq();
    }

    void unlock() override 
    { 
        __set_PRIMASK(_primaskSave);
    }
};

} // namespace PHL
