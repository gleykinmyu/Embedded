#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/gpio.h"
#include "phl.h"
#include "core/property.hpp"

namespace UART {

namespace detail {

/// Частота тактирования USART по PHL::ID (как HAL UART_SetConfig: USART1/USART6 — APB2, остальные — APB1).
template <PHL::ID Id>
inline uint32_t usart_input_clock_hz() noexcept
{
    constexpr uintptr_t base = static_cast<uintptr_t>(Id);
#ifdef USART1_BASE
    if (base == USART1_BASE)
        return HAL_RCC_GetPCLK2Freq();
#endif
#ifdef USART6_BASE
    if (base == USART6_BASE)
        return HAL_RCC_GetPCLK2Freq();
#endif
    return HAL_RCC_GetPCLK1Freq();
}

/// BRR при передискретизации 16× (логика UART_BRR_SAMPLING16 из stm32f4xx_hal_uart.h).
inline uint32_t brr_sampling16(uint32_t pclk_hz, uint32_t baud) noexcept
{
    if (baud == 0U)
        return 0U;
    const uint64_t div = (static_cast<uint64_t>(pclk_hz) * 25ULL) / (4ULL * static_cast<uint64_t>(baud));
    const uint32_t d = static_cast<uint32_t>(div);
    const uint32_t mant = d / 100U;
    const uint32_t frac_cent = d - mant * 100U;
    const uint32_t frac = ((frac_cent * 16U) + 50U) / 100U;
    return (mant << 4U) + (frac & 0xF0U) + (frac & 0x0FU);
}

/// BRR при передискретизации 8× (UART_BRR_SAMPLING8).
inline uint32_t brr_sampling8(uint32_t pclk_hz, uint32_t baud) noexcept
{
    if (baud == 0U)
        return 0U;
    const uint64_t div = (static_cast<uint64_t>(pclk_hz) * 25ULL) / (2ULL * static_cast<uint64_t>(baud));
    const uint32_t d = static_cast<uint32_t>(div);
    const uint32_t mant = d / 100U;
    const uint32_t frac_cent = d - mant * 100U;
    const uint32_t frac = ((frac_cent * 8U) + 50U) / 100U;
    return (mant << 4U) + ((frac & 0xF8U) << 1U) + (frac & 0x07U);
}

} // namespace detail

/// Маски USART->SR (регистр состояния).
enum class SR : uint32_t {
    PE   = USART_SR_PE,   ///< PE: ошибка чётности (parity error).
    FE   = USART_SR_FE,   ///< FE: ошибка кадра (framing error).
    NE   = USART_SR_NE,   ///< NE: обнаружен шум на линии (noise error).
    ORE  = USART_SR_ORE,  ///< ORE: переполнение приёма (overrun).
    IDLE = USART_SR_IDLE, ///< IDLE: линия в состоянии простоя (idle line detected).
    RXNE = USART_SR_RXNE, ///< RXNE: в DR есть принятый байт (read data register not empty).
    TC   = USART_SR_TC,   ///< TC: сдвиговый регистр опустошён, передача завершена.
    TXE  = USART_SR_TXE,  ///< TXE: DR пуст, можно писать следующий байт (transmit data register empty).
};

/// Маски USART->CR1 (управление и прерывания; подмножество CMSIS).
enum class CR1 : uint32_t {
    SBK    = USART_CR1_SBK,    ///< SBK: запрос послать break (последовательность «0»).
    RWU    = USART_CR1_RWU,    ///< RWU: приёмник в режиме mute / пробуждение по адресу.
    RE     = USART_CR1_RE,     ///< RE: включить приёмник.
    TE     = USART_CR1_TE,     ///< TE: включить передатчик.
    IDLEIE = USART_CR1_IDLEIE, ///< IDLEIE: прерывание по флагу IDLE.
    RXNEIE = USART_CR1_RXNEIE, ///< RXNEIE: прерывание при непустом DR (RXNE).
    TCIE   = USART_CR1_TCIE,   ///< TCIE: прерывание по завершении передачи (TC).
    TXEIE  = USART_CR1_TXEIE,  ///< TXEIE: прерывание при пустом DR (TXE).
    PEIE   = USART_CR1_PEIE,   ///< PEIE: прерывание при ошибке чётности (PE).
    PS     = USART_CR1_PS,     ///< PS: выбор чётность/нечётность (вместе с PCE).
    PCE    = USART_CR1_PCE,    ///< PCE: включить контроль чётности.
    M      = USART_CR1_M,      ///< M: длина слова (0 — 8 бит данных, 1 — 9 бит).
    UE     = USART_CR1_UE,     ///< UE: включить USART.
    OVER8  = USART_CR1_OVER8,  ///< OVER8: передискретизация 8× вместо 16×.
};

/// Маски USART->CR2. STOP_0 / STOP_1 — два бита поля STOP[1:0] (как CMSIS); 1 стоп = 0, 2 стопа = STOP_1,
/// 0.5 / 1.5 — только STOP_0 / (STOP_0|STOP_1). Не путать с USART_CR2_STOP — это маска всего поля (0x3000).
enum class CR2 : uint32_t {
    STOP_0 = USART_CR2_STOP_0, ///< STOP[0]: в паре со STOP_1 задаёт число стоп-битов.
    STOP_1 = USART_CR2_STOP_1, ///< STOP[1]: два стоп-бита — только STOP_1; не OR-ить «USART_CR2_STOP» целиком.
    LBDL   = USART_CR2_LBDL,   ///< LBDL: длина break-символа в LIN (11 или 10 бит).
    LBDIE  = USART_CR2_LBDIE,  ///< LBDIE: прерывание при обнаружении LIN break.
    LBCL   = USART_CR2_LBCL,   ///< LBCL: импульс такта на последнем бите данных.
    CPHA   = USART_CR2_CPHA,   ///< CPHA: фаза такта SCLK (синхронный режим).
    CPOL   = USART_CR2_CPOL,   ///< CPOL: полярность такта SCLK.
    CLKEN  = USART_CR2_CLKEN,  ///< CLKEN: выход тактового сигнала SCLK.
    LINEN  = USART_CR2_LINEN,  ///< LINEN: режим LIN.
};

/// Маски USART->CR3 (DMA, IrDA, смарт-карта, RTS/CTS).
enum class CR3 : uint32_t {
    EIE    = USART_CR3_EIE,    ///< EIE: прерывания по ошибкам FE/NE/ORE (в режиме DMA).
    IREN   = USART_CR3_IREN,   ///< IREN: режим IrDA.
    IRLP   = USART_CR3_IRLP,   ///< IRLP: IrDA low-power (узкий импульс).
    HDSEL  = USART_CR3_HDSEL,  ///< HDSEL: полудуплекс (одна линия TX/RX).
    NACK   = USART_CR3_NACK,   ///< NACK: NACK в режиме смарт-карты.
    SCEN   = USART_CR3_SCEN,   ///< SCEN: режим смарт-карты ISO7816.
    DMAR   = USART_CR3_DMAR,   ///< DMAR: DMA на приём.
    DMAT   = USART_CR3_DMAT,   ///< DMAT: DMA на передачу.
    RTSE   = USART_CR3_RTSE,   ///< RTSE: аппаратный RTS (управление потоком).
    CTSE   = USART_CR3_CTSE,   ///< CTSE: аппаратный CTS.
    CTSIE  = USART_CR3_CTSIE,  ///< CTSIE: прерывание по изменению CTS.
    ONEBIT = USART_CR3_ONEBIT, ///< ONEBIT: один выборочный момент вместо трёх (шумоустойчивость).
};

REG_BITMASK_ENUM_OPS(SR)
REG_BITMASK_ENUM_OPS(CR1)
REG_BITMASK_ENUM_OPS(CR2)
REG_BITMASK_ENUM_OPS(CR3)

/**
 * USART->CR2: REG::PropertyBits<CR2, …> плюс поле CR2.ADD = Address of the USART node (RM).
 * Mute / multiprocessor wake-up. Ширина ADD — USART_CR2_ADD_Msk из CMSIS для выбранного MCU.
 */
template <PHL::ID UartId>
class PropertyCR2 : public REG::PropertyBits<CR2, UartId, offsetof(USART_TypeDef, CR2)> {
    using Base = REG::PropertyBits<CR2, UartId, offsetof(USART_TypeDef, CR2)>;

public:
    using Base::read;
    using Base::read_raw;
    using Base::write;
    using Base::set;
    using Base::clear;
    using Base::any;
    using Base::all;
    using Base::get;
    using Base::addr;

    /// CR2.ADD — адрес узла (не «сложение»). Желательно при UE=0.
    void setAddress(uint32_t value) noexcept
    {
        const uint32_t v = Base::read_raw();
        const uint32_t field =
            (value << USART_CR2_ADD_Pos) & static_cast<uint32_t>(USART_CR2_ADD_Msk);
        Base::write((v & ~static_cast<uint32_t>(USART_CR2_ADD_Msk)) | field);
    }

    /// Текущее значение CR2.ADD (адрес узла).
    uint32_t address() const noexcept
    {
        return (Base::read_raw() & static_cast<uint32_t>(USART_CR2_ADD_Msk)) >> USART_CR2_ADD_Pos;
    }
};

// ---------------------------------------------------------------------------
// Формат асинхронного кадра (скорость — отдельным uint32_t в ConfigureAsync).
// ---------------------------------------------------------------------------

enum class DataBits : uint8_t {
    Bits8 = 8,
    Bits9 = 9,
};

enum class Parity : uint8_t {
    None,
    Even,
    Odd,
};

enum class StopBits : uint8_t {
    One = 1,
    Two = 2,
};

enum class Oversampling : uint8_t {
    x16,
    x8,
};

/**
 * Формат кадра; пресеты _8N1, _8E1, … Соответствие CR1/CR2 — как HAL UART_SetConfig (STM32F4).
 */
struct Frame {
    DataBits data_bits;
    Parity parity;
    StopBits stop_bits;
    Oversampling oversampling;

    static constexpr Frame make(DataBits d, Parity p, StopBits s,
                                Oversampling o = Oversampling::x16) noexcept
    {
        return {d, p, s, o};
    }

    static constexpr Frame _8N1() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::One, Oversampling::x16);
    }

    static constexpr Frame _8E1() noexcept
    {
        return make(DataBits::Bits8, Parity::Even, StopBits::One, Oversampling::x16);
    }

    static constexpr Frame _8O1() noexcept
    {
        return make(DataBits::Bits8, Parity::Odd, StopBits::One, Oversampling::x16);
    }

    static constexpr Frame _8N2() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::Two, Oversampling::x16);
    }

    static constexpr Frame _8E2() noexcept
    {
        return make(DataBits::Bits8, Parity::Even, StopBits::Two, Oversampling::x16);
    }

    static constexpr Frame _8O2() noexcept
    {
        return make(DataBits::Bits8, Parity::Odd, StopBits::Two, Oversampling::x16);
    }

    static constexpr Frame _9N1() noexcept
    {
        return make(DataBits::Bits9, Parity::None, StopBits::One, Oversampling::x16);
    }

    static constexpr Frame _9E1() noexcept
    {
        return make(DataBits::Bits9, Parity::Even, StopBits::One, Oversampling::x16);
    }

    static constexpr Frame _9O1() noexcept
    {
        return make(DataBits::Bits9, Parity::Odd, StopBits::One, Oversampling::x16);
    }

    static constexpr Frame _9N2() noexcept
    {
        return make(DataBits::Bits9, Parity::None, StopBits::Two, Oversampling::x16);
    }

    static constexpr Frame _9E2() noexcept
    {
        return make(DataBits::Bits9, Parity::Even, StopBits::Two, Oversampling::x16);
    }

    static constexpr Frame _9O2() noexcept
    {
        return make(DataBits::Bits9, Parity::Odd, StopBits::Two, Oversampling::x16);
    }
};

/**
 * USART/UART по PHL::ID.
 * sr, cr1, cr3 — REG::PropertyBits; cr2 — PropertyCR2 (ADD); dr, brr — REG::PropertyWord.
 * ConfigureAsync(baud, Frame); сокращение ConfigureAsync8N1(baud) = 8N1.
 */
template <PHL::ID UartId>
class UART : public PHL::IBase<UartId> {
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "UART: только PHL::ID с Type::UART");

public:
    USART_TypeDef* const instance;

    REG::PropertyBits<SR, UartId, offsetof(USART_TypeDef, SR)> sr;
    REG::PropertyBits<CR1, UartId, offsetof(USART_TypeDef, CR1)> cr1;
    PropertyCR2<UartId> cr2;
    REG::PropertyBits<CR3, UartId, offsetof(USART_TypeDef, CR3)> cr3;
    REG::PropertyWord<UartId, offsetof(USART_TypeDef, DR)> dr;
    REG::PropertyWord<UartId, offsetof(USART_TypeDef, BRR)> brr;

    UART()
        : instance(reinterpret_cast<USART_TypeDef*>(static_cast<uintptr_t>(UartId)))
    {
    }

    /// TX/RX в режиме AF PP; подтяжка и скорость — как у типичного UART.
    void InitPins(const GPIO::Pin& tx, const GPIO::Pin& rx) const
    {
        tx.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        rx.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
    }

    /**
     * Асинхронный режим: baud_hz + кадр. Перед вызовом: EnableClock().
     */
    bool ConfigureAsync(uint32_t baud_hz, const Frame& fmt = Frame::_8N1()) noexcept
    {
        if (baud_hz == 0U)
            return false;

        const uint32_t pclk = detail::usart_input_clock_hz<UartId>();
        if (pclk == 0U)
            return false;

        cr1.clear(CR1::UE);

        const REG::BitMask<CR2> cr2_async_clr = CR2::STOP_0 | CR2::STOP_1 | CR2::LINEN | CR2::CLKEN;
        REG::BitMask<CR2> cr2_cfg;
        if (fmt.stop_bits == StopBits::Two)
            cr2_cfg.set(CR2::STOP_1);

        REG::BitMask<CR2> r2 = cr2.read();
        r2.clear(cr2_async_clr);
        r2.set(cr2_cfg);
        cr2.write(r2);

        const REG::BitMask<CR1> cr1_async_clr = CR1::M | CR1::PCE | CR1::PS | CR1::OVER8 | CR1::TE | CR1::RE;
        REG::BitMask<CR1> cr1_cfg;
        if (fmt.data_bits == DataBits::Bits9)
            cr1_cfg = cr1_cfg | CR1::M;
        if (fmt.parity != Parity::None) {
            cr1_cfg = cr1_cfg | CR1::PCE;
            if (fmt.parity == Parity::Odd)
                cr1_cfg = cr1_cfg | CR1::PS;
        }
        cr1_cfg = cr1_cfg | CR1::TE | CR1::RE;
        if (fmt.oversampling == Oversampling::x8)
            cr1_cfg = cr1_cfg | CR1::OVER8;

        REG::BitMask<CR1> r1 = cr1.read();
        r1.clear(cr1_async_clr);
        r1.set(cr1_cfg);
        cr1.write(r1);

        const REG::BitMask<CR3> cr3_async_clr =
            CR3::RTSE | CR3::CTSE | CR3::SCEN | CR3::HDSEL | CR3::IREN;
        cr3.clear(cr3_async_clr);
        
        const uint32_t brr_val = (fmt.oversampling == Oversampling::x8)
                                     ? detail::brr_sampling8(pclk, baud_hz)
                                     : detail::brr_sampling16(pclk, baud_hz);
        if (brr_val == 0U)
            return false;
        brr.write(brr_val);

        cr1.set(CR1::UE);
        return true;
    }

    /// Выключить USART (UE=0). Прерывания в CR1 снимает вызывающий.
    void Shutdown() noexcept { cr1.clear(CR1::UE); }
};

} // namespace UART
