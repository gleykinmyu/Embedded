#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/gpio.h"
#include "phl.h"
#include "core/property.hpp"

namespace SPI {

namespace detail {

/**
 * Поле CR1.BR[2:0]: делитель SCK = 2<<(BR).
 * Выбираем наибольший делитель, при котором actual = pclk/div не выше baud_hz;
 * если даже /2 уже выше — BR=0.
 */
inline uint32_t br_bits_for_baud(uint32_t pclk_hz, uint32_t baud_hz) noexcept
{
    if (baud_hz == 0U || pclk_hz == 0U)
        return 7U;

    uint32_t br = 0U;
    for (; br < 7U; ++br) {
        const uint32_t div = 2U << br;
        if ((pclk_hz / div) <= baud_hz)
            break;
    }
    return br;
}

} // namespace detail

/// Маски SPI->SR.
enum class SR : uint32_t {
    RXNE = SPI_SR_RXNE, ///< RXNE: в DR есть принятый байт.
    TXE  = SPI_SR_TXE,  ///< TXE: DR пуст, можно писать следующий байт.
    UDR  = SPI_SR_UDR,  ///< UDR: underrun (I2S).
    CRCERR = SPI_SR_CRCERR,
    MODF = SPI_SR_MODF, ///< MODF: mode fault (master NSS).
    OVR  = SPI_SR_OVR,  ///< OVR: overrun приёма.
    BSY  = SPI_SR_BSY,  ///< BSY: идёт обмен / занят сдвиговый регистр.
};

/// Маски SPI->CR1.
enum class CR1 : uint32_t {
    CPHA     = SPI_CR1_CPHA,     ///< CPHA: фаза SCK.
    CPOL     = SPI_CR1_CPOL,     ///< CPOL: полярность SCK.
    MSTR     = SPI_CR1_MSTR,     ///< MSTR: master.
    BR_0     = SPI_CR1_BR_0,     ///< BR[0]: делитель такта.
    BR_1     = SPI_CR1_BR_1,     ///< BR[1].
    BR_2     = SPI_CR1_BR_2,     ///< BR[2].
    SPE      = SPI_CR1_SPE,      ///< SPE: включить SPI.
    LSBFIRST = SPI_CR1_LSBFIRST, ///< LSBFIRST: младший бит первым.
    SSI      = SPI_CR1_SSI,      ///< SSI: внутренний уровень NSS (при SSM=1).
    SSM      = SPI_CR1_SSM,      ///< SSM: software slave management.
    RXONLY   = SPI_CR1_RXONLY,   ///< RXONLY: только приём.
    DFF      = SPI_CR1_DFF,      ///< DFF: 16-бит кадр вместо 8.
    CRCNEXT  = SPI_CR1_CRCNEXT,
    CRCEN    = SPI_CR1_CRCEN,
    BIDIOE   = SPI_CR1_BIDIOE,
    BIDIMODE = SPI_CR1_BIDIMODE,
};

/// Маски SPI->CR2.
enum class CR2 : uint32_t {
    RXDMAEN = SPI_CR2_RXDMAEN,
    TXDMAEN = SPI_CR2_TXDMAEN,
    SSOE    = SPI_CR2_SSOE,    ///< SSOE: выход NSS в master (аппаратный NSS).
    ERRIE   = SPI_CR2_ERRIE,   ///< ERRIE: прерывание по ошибкам.
    RXNEIE  = SPI_CR2_RXNEIE,  ///< RXNEIE: прерывание при RXNE.
    TXEIE   = SPI_CR2_TXEIE,   ///< TXEIE: прерывание при TXE.
};

REG_BITMASK_ENUM_OPS(SR)
REG_BITMASK_ENUM_OPS(CR1)
REG_BITMASK_ENUM_OPS(CR2)

/// Ширина кадра данных.
enum class DataBits : uint8_t {
    Bits8  = 8,
    Bits16 = 16,
};

/// CPOL: уровень SCK в idle.
enum class ClockPolarity : uint8_t {
    IdleLow  = 0, ///< CPOL=0: SCK низкий в простое.
    IdleHigh = 1, ///< CPOL=1: SCK высокий в простое.
};

/// CPHA: фронт выборки данных.
enum class ClockPhase : uint8_t {
    FirstEdge  = 0, ///< CPHA=0: выборка по первому фронту.
    SecondEdge = 1, ///< CPHA=1: выборка по второму фронту.
};

/**
 * Режим SPI (CPOL/CPHA + порядок бит + ширина).
 * Пресеты Mode0..Mode3 — как у Winbond W25Q (обычно Mode0 или Mode3).
 */
struct Mode {
    ClockPolarity clock_polarity;
    ClockPhase clock_phase;
    bool lsb_first; ///< LSBFIRST: младший бит первым.
    DataBits data_bits;

    static constexpr Mode make(ClockPolarity polarity, ClockPhase phase, bool lsb = false,
                               DataBits d = DataBits::Bits8) noexcept
    {
        return {polarity, phase, lsb, d};
    }

    /// CPOL=0, CPHA=0.
    static constexpr Mode Mode0() noexcept
    {
        return make(ClockPolarity::IdleLow, ClockPhase::FirstEdge);
    }
    /// CPOL=0, CPHA=1.
    static constexpr Mode Mode1() noexcept
    {
        return make(ClockPolarity::IdleLow, ClockPhase::SecondEdge);
    }
    /// CPOL=1, CPHA=0.
    static constexpr Mode Mode2() noexcept
    {
        return make(ClockPolarity::IdleHigh, ClockPhase::FirstEdge);
    }
    /// CPOL=1, CPHA=1.
    static constexpr Mode Mode3() noexcept
    {
        return make(ClockPolarity::IdleHigh, ClockPhase::SecondEdge);
    }
};

/**
 * SPI по PHL::ID.
 * cr1/cr2/sr — REG::PropertyBits; dr — REG::PropertyWord.
 * ConfigureMaster(baud, Mode); soft NSS (SSM+SSI) — CS снаружи GPIO.
 */
template <PHL::ID SpiId>
class SPI : public PHL::IBase<SpiId> {
    static_assert(PHL::GetType<SpiId>::value == PHL::Type::SPI, "SPI: только PHL::ID с Type::SPI");

public:
    SPI_TypeDef* const instance;

    REG::PropertyBits<CR1, SpiId, offsetof(SPI_TypeDef, CR1)> cr1;
    REG::PropertyBits<CR2, SpiId, offsetof(SPI_TypeDef, CR2)> cr2;
    REG::PropertyBits<SR, SpiId, offsetof(SPI_TypeDef, SR)> sr;
    REG::PropertyWord<SpiId, offsetof(SPI_TypeDef, DR)> dr;

    SPI()
        : instance(reinterpret_cast<SPI_TypeDef*>(static_cast<uintptr_t>(SpiId)))
    {
    }

    /// SCK / MISO / MOSI в AF; NSS (CS) — soft GPIO снаружи (не здесь).
    void InitPins(const GPIO::Pin& sck, const GPIO::Pin& miso, const GPIO::Pin& mosi) const
    {
        sck.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        miso.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        mosi.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
    }

    /**
     * Master + soft NSS (SSM=1, SSI=1): baud_hz + Mode.
     * Перед вызовом: EnableClock().
     */
    bool ConfigureMaster(uint32_t baud_hz, const Mode& fmt = Mode::Mode0()) noexcept
    {
        if (baud_hz == 0U)
            return false;

        const uint32_t pclk = this->InputClockHz();
        if (pclk == 0U)
            return false;

        cr1.clear(CR1::SPE);

        const uint32_t br = detail::br_bits_for_baud(pclk, baud_hz);
        REG::BitMask<CR1> cfg = CR1::MSTR | CR1::SSM | CR1::SSI;
        if (fmt.clock_phase == ClockPhase::SecondEdge)
            cfg = cfg | CR1::CPHA;
        if (fmt.clock_polarity == ClockPolarity::IdleHigh)
            cfg = cfg | CR1::CPOL;
        if (fmt.lsb_first)
            cfg = cfg | CR1::LSBFIRST;
        if (fmt.data_bits == DataBits::Bits16)
            cfg = cfg | CR1::DFF;
        if ((br & 1U) != 0U)
            cfg = cfg | CR1::BR_0;
        if ((br & 2U) != 0U)
            cfg = cfg | CR1::BR_1;
        if ((br & 4U) != 0U)
            cfg = cfg | CR1::BR_2;

        const REG::BitMask<CR1> cr1_clr = CR1::CPHA | CR1::CPOL | CR1::MSTR | CR1::BR_0 | CR1::BR_1 |
                                          CR1::BR_2 | CR1::LSBFIRST | CR1::SSI | CR1::SSM | CR1::RXONLY |
                                          CR1::DFF | CR1::CRCEN | CR1::BIDIMODE | CR1::BIDIOE;
        REG::BitMask<CR1> r1 = cr1.read();
        r1.clear(cr1_clr);
        r1.set(cfg);
        cr1.write(r1);

        // Soft NSS: не SSOE (CS — отдельный GPIO).
        cr2.clear(CR2::SSOE | CR2::RXDMAEN | CR2::TXDMAEN);

        cr1.set(CR1::SPE);
        return true;
    }

    /// Выключить               ЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁЁ     (SPE=0). Прерывания в CR2 снимает вызывающий.
    void Shutdown() noexcept { cr1.clear(CR1::SPE); }

    /**
     * Полный дуплекс: один байт MOSI → один байт MISO (polling).
     * SPE должен быть включён.
     */
    uint8_t transfer(uint8_t tx) noexcept
    {
        while (!sr.any(SR::TXE)) {
        }
        dr.write(static_cast<uint32_t>(tx));
        while (!sr.any(SR::RXNE)) {
        }
        return static_cast<uint8_t>(dr.read() & 0xFFU);
    }
};

} // namespace SPI
