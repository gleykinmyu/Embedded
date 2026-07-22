#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core/gpio.h"
#include "phl.h"
#include "core/property.hpp"
#include "ican.hpp"

namespace CAN {

namespace detail {

inline uint32_t abs_diff_u32(uint32_t a, uint32_t b) noexcept
{
    return (a > b) ? (a - b) : (b - a);
}

inline constexpr uint32_t kModeWaitSpins = 1000000U;

} // namespace detail

using Frame = BIF::CAN::Frame;
using FilterMode = BIF::CAN::FilterMode;
using FilterScale = BIF::CAN::FilterScale;
using FilterFifo = BIF::CAN::FilterFifo;

/// Маски CAN->MCR.
enum class MCR : uint32_t {
    INRQ  = CAN_MCR_INRQ,  ///< INRQ: запрос режима инициализации.
    SLEEP = CAN_MCR_SLEEP, ///< SLEEP: запрос sleep.
    TXFP  = CAN_MCR_TXFP,  ///< TXFP: приоритет TX по номеру mailbox, не по ID.
    RFLM  = CAN_MCR_RFLM,  ///< RFLM: FIFO locked при overrun.
    NART  = CAN_MCR_NART,  ///< NART: без автоповтора TX.
    AWUM  = CAN_MCR_AWUM,  ///< AWUM: авто-выход из sleep по шине.
    ABOM  = CAN_MCR_ABOM,  ///< ABOM: авто bus-off recovery.
    TTCM  = CAN_MCR_TTCM,  ///< TTCM: time-triggered mode.
    RESET = CAN_MCR_RESET, ///< RESET: программный сброс bxCAN.
    DBF   = CAN_MCR_DBF,   ///< DBF: freeze в debug halt.
};

/// Маски CAN->MSR.
enum class MSR : uint32_t {
    INAK  = CAN_MSR_INAK,  ///< INAK: подтверждение init mode.
    SLAK  = CAN_MSR_SLAK,  ///< SLAK: подтверждение sleep.
    ERRI  = CAN_MSR_ERRI,  ///< ERRI: флаг error interrupt.
    WKUI  = CAN_MSR_WKUI,  ///< WKUI: wakeup interrupt.
    SLAKI = CAN_MSR_SLAKI, ///< SLAKI: sleep acknowledge interrupt.
    TXM   = CAN_MSR_TXM,   ///< TXM: идёт передача.
    RXM   = CAN_MSR_RXM,   ///< RXM: идёт приём.
    SAMP  = CAN_MSR_SAMP,  ///< SAMP: последний sample point.
    RX    = CAN_MSR_RX,    ///< RX: уровень линии RX.
};

/// Маски CAN->TSR (подмножество: empty + request complete).
enum class TSR : uint32_t {
    RQCP0 = CAN_TSR_RQCP0,
    TXOK0 = CAN_TSR_TXOK0,
    ALST0 = CAN_TSR_ALST0,
    TERR0 = CAN_TSR_TERR0,
    ABRQ0 = CAN_TSR_ABRQ0,
    RQCP1 = CAN_TSR_RQCP1,
    TXOK1 = CAN_TSR_TXOK1,
    ALST1 = CAN_TSR_ALST1,
    TERR1 = CAN_TSR_TERR1,
    ABRQ1 = CAN_TSR_ABRQ1,
    RQCP2 = CAN_TSR_RQCP2,
    TXOK2 = CAN_TSR_TXOK2,
    ALST2 = CAN_TSR_ALST2,
    TERR2 = CAN_TSR_TERR2,
    ABRQ2 = CAN_TSR_ABRQ2,
    TME0  = CAN_TSR_TME0, ///< TME0: mailbox 0 свободен.
    TME1  = CAN_TSR_TME1,
    TME2  = CAN_TSR_TME2,
};

/// Маски CAN->RF0R.
enum class RF0R : uint32_t {
    FMP0  = CAN_RF0R_FMP0,  ///< FMP0[1:0]: число сообщений в FIFO0.
    FULL0 = CAN_RF0R_FULL0,
    FOVR0 = CAN_RF0R_FOVR0,
    RFOM0 = CAN_RF0R_RFOM0, ///< RFOM0: освободить выходной mailbox FIFO0.
};

/// Маски CAN->IER.
enum class IER : uint32_t {
    TMEIE  = CAN_IER_TMEIE,
    FMPIE0 = CAN_IER_FMPIE0,
    FFIE0  = CAN_IER_FFIE0,
    FOVIE0 = CAN_IER_FOVIE0,
    FMPIE1 = CAN_IER_FMPIE1,
    FFIE1  = CAN_IER_FFIE1,
    FOVIE1 = CAN_IER_FOVIE1,
    EWGIE  = CAN_IER_EWGIE,
    EPVIE  = CAN_IER_EPVIE,
    BOFIE  = CAN_IER_BOFIE,
    LECIE  = CAN_IER_LECIE,
    ERRIE  = CAN_IER_ERRIE,
    WKUIE  = CAN_IER_WKUIE,
    SLKIE  = CAN_IER_SLKIE,
};

/// Маски CAN->ESR (флаги; TEC/REC — поля, читайте через esr.read_raw()).
enum class ESR : uint32_t {
    EWGF = CAN_ESR_EWGF,
    EPVF = CAN_ESR_EPVF,
    BOFF = CAN_ESR_BOFF,
};

/// Маски CAN->BTR (режимы; BRP/TS1/TS2/SJW — через BitTiming::toBtr()).
enum class BTR : uint32_t {
    LBKM = CAN_BTR_LBKM, ///< LBKM: loopback.
    SILM = CAN_BTR_SILM, ///< SILM: silent (listen-only).
};

/// Маски CAN->FMR.
enum class FMR : uint32_t {
    FINIT = CAN_FMR_FINIT, ///< FINIT: настройка filter bank.
};

REG_BITMASK_ENUM_OPS(MCR)
REG_BITMASK_ENUM_OPS(MSR)
REG_BITMASK_ENUM_OPS(TSR)
REG_BITMASK_ENUM_OPS(RF0R)
REG_BITMASK_ENUM_OPS(IER)
REG_BITMASK_ENUM_OPS(ESR)
REG_BITMASK_ENUM_OPS(BTR)
REG_BITMASK_ENUM_OPS(FMR)

/// Число filter bank’ов bxCAN (F4: 28, биты в FM1R/FS1R/FFA1R/FA1R).
inline constexpr uint8_t kFilterBankCount = 28;

/**
 * Bit timing bxCAN.
 * Поля — «человеческие» (1-based): в BTR пишется (value - 1).
 * prescaler == 0 в Configure означает «подобрать forBaud».
 */
struct BitTiming {
    uint16_t prescaler = 0;     ///< 1…1024; 0 = авто в Configure(baud).
    uint8_t syncJumpWidth = 1;  ///< SJW: 1…4 tq (ресинхронизация).
    uint8_t timeSeg1 = 13;      ///< BS1 / TS1: 1…16 tq (до sample point).
    uint8_t timeSeg2 = 2;       ///< BS2 / TS2: 1…8 tq (после sample point).
    bool loopback = false;
    bool silent = false;

    [[nodiscard]] uint32_t toBtr() const noexcept
    {
        uint32_t v = ((static_cast<uint32_t>(prescaler) - 1U) & CAN_BTR_BRP)
                     | (((static_cast<uint32_t>(timeSeg1) - 1U) << CAN_BTR_TS1_Pos) & CAN_BTR_TS1)
                     | (((static_cast<uint32_t>(timeSeg2) - 1U) << CAN_BTR_TS2_Pos) & CAN_BTR_TS2)
                     | (((static_cast<uint32_t>(syncJumpWidth) - 1U) << CAN_BTR_SJW_Pos) & CAN_BTR_SJW);
        if (loopback)
            v |= CAN_BTR_LBKM;
        if (silent)
            v |= CAN_BTR_SILM;
        return v;
    }

    /**
     * Подобрать BRP/TS1/TS2 под baud при sample point ~75%.
     * Предпочитает точное совпадение; иначе минимальная ошибка частоты.
     */
    [[nodiscard]] static bool forBaud(uint32_t pclk_hz, uint32_t baud_hz, BitTiming& out) noexcept
    {
        if (pclk_hz == 0U || baud_hz == 0U)
            return false;

        uint32_t best_err = 0xFFFFFFFFU;
        BitTiming best{};
        bool found = false;

        // ntq = 1 (sync) + timeSeg1 + timeSeg2; типично 8…25. Sample point ≈ 75%.
        for (uint32_t ntq = 25U; ntq >= 8U; --ntq) {
            const uint32_t brp = pclk_hz / (baud_hz * ntq);
            if (brp < 1U || brp > 1024U)
                continue;

            // sample_quanta = 1 + timeSeg1 ≈ 0.75 * ntq → timeSeg1 = round(0.75*ntq) - 1.
            uint32_t seg1 = ((ntq * 3U) / 4U);
            if (seg1 > 0U)
                --seg1;
            if (seg1 < 1U)
                seg1 = 1U;
            if (seg1 > 16U)
                seg1 = 16U;

            const uint32_t seg2 = ntq - 1U - seg1;
            if (seg2 < 1U || seg2 > 8U)
                continue;

            uint32_t sjw = seg2;
            if (sjw > 4U)
                sjw = 4U;

            const uint32_t actual = pclk_hz / (brp * ntq);
            const uint32_t err = detail::abs_diff_u32(actual, baud_hz);
            if (err < best_err) {
                best_err = err;
                best.prescaler = static_cast<uint16_t>(brp);
                best.syncJumpWidth = static_cast<uint8_t>(sjw);
                best.timeSeg1 = static_cast<uint8_t>(seg1);
                best.timeSeg2 = static_cast<uint8_t>(seg2);
                best.loopback = out.loopback;
                best.silent = out.silent;
                found = true;
                if (err == 0U)
                    break;
            }
        }

        if (!found)
            return false;
        out = best;
        return true;
    }
};

/**
 * bxCAN по PHL::ID.
 * mcr/msr/… — REG::PropertyBits; mailbox — instance->sTxMailBox / sFIFOMailBox.
 * Фильтры — can.filter[i].setActive / mode / …; tryTransmit / tryReceive (FIFO0).
 */
template <PHL::ID CanId>
class CAN : public PHL::IBase<CanId> {
    static_assert(PHL::GetType<CanId>::value == PHL::Type::CAN, "CAN: только PHL::ID с Type::CAN");

public:
    CAN_TypeDef* const instance;

    REG::PropertyBits<MCR, CanId, offsetof(CAN_TypeDef, MCR)> mcr;
    REG::PropertyBits<MSR, CanId, offsetof(CAN_TypeDef, MSR)> msr;
    REG::PropertyBits<TSR, CanId, offsetof(CAN_TypeDef, TSR)> tsr;
    REG::PropertyBits<RF0R, CanId, offsetof(CAN_TypeDef, RF0R)> rf0r;
    REG::PropertyBits<IER, CanId, offsetof(CAN_TypeDef, IER)> ier;
    REG::PropertyBits<ESR, CanId, offsetof(CAN_TypeDef, ESR)> esr;
    /// BTR целиком (toBtr) или биты LBKM/SILM через set/clear.
    REG::PropertyBits<BTR, CanId, offsetof(CAN_TypeDef, BTR)> btr;

    /**
     * Хелпер filter bank’ов: `can.filter[i].setActive(true)`.
     * Хранит instance + FMR/FM1R/FS1R/FFA1R/FA1R.
     * Запись конфигурации — только при FINIT: initEnter() … initLeave().
     * Чтение (isActive / mode / fr1 / …) FINIT не требует.
     */
    class Filter {
    public:
        CAN_TypeDef* const instance;

        REG::PropertyBits<FMR, CanId, offsetof(CAN_TypeDef, FMR)> fmr;
        REG::PropertyWord<CanId, offsetof(CAN_TypeDef, FM1R)> fm1r;
        REG::PropertyWord<CanId, offsetof(CAN_TypeDef, FS1R)> fs1r;
        REG::PropertyWord<CanId, offsetof(CAN_TypeDef, FFA1R)> ffa1r;
        REG::PropertyWord<CanId, offsetof(CAN_TypeDef, FA1R)> fa1r;

        explicit Filter(CAN_TypeDef* inst) noexcept : instance(inst) {}

        void initEnter() noexcept { fmr.set(FMR::FINIT); }
        void initLeave() noexcept { fmr.clear(FMR::FINIT); }

        /// Прокси одного bank: `filter[3].setMode(…)`.
        class Bank {
            Filter& _filters;
            uint8_t _index;

        public:
            Bank(Filter& filters, uint8_t index) noexcept : _filters(filters), _index(index) {}

            [[nodiscard]] uint8_t index() const noexcept { return _index; }
            [[nodiscard]] bool valid() const noexcept { return _index < kFilterBankCount; }

            [[nodiscard]] bool setActive(bool active) noexcept
            {
                if (!valid())
                    return false;
                _filters.fa1r.writeBit(_index, active);
                return true;
            }

            [[nodiscard]] bool isActive() const noexcept
            {
                return valid() && _filters.fa1r.readBit(_index);
            }

            [[nodiscard]] bool setMode(FilterMode mode) noexcept
            {
                if (!valid())
                    return false;
                _filters.fm1r.writeBit(_index, mode == FilterMode::List);
                return true;
            }

            [[nodiscard]] FilterMode mode() const noexcept
            {
                return (valid() && _filters.fm1r.readBit(_index)) ? FilterMode::List : FilterMode::Mask;
            }

            [[nodiscard]] bool setScale(FilterScale scale) noexcept
            {
                if (!valid())
                    return false;
                _filters.fs1r.writeBit(_index, scale == FilterScale::Single32);
                return true;
            }

            [[nodiscard]] FilterScale scale() const noexcept
            {
                return (valid() && _filters.fs1r.readBit(_index)) ? FilterScale::Single32
                                                                 : FilterScale::Dual16;
            }

            [[nodiscard]] bool setFifo(FilterFifo fifo) noexcept
            {
                if (!valid())
                    return false;
                _filters.ffa1r.writeBit(_index, fifo == FilterFifo::Fifo1);
                return true;
            }

            [[nodiscard]] FilterFifo fifo() const noexcept
            {
                return (valid() && _filters.ffa1r.readBit(_index)) ? FilterFifo::Fifo1
                                                                  : FilterFifo::Fifo0;
            }

            [[nodiscard]] bool write(uint32_t fr1, uint32_t fr2) noexcept
            {
                if (!valid())
                    return false;
                _filters.instance->sFilterRegister[_index].FR1 = fr1;
                _filters.instance->sFilterRegister[_index].FR2 = fr2;
                return true;
            }

            [[nodiscard]] uint32_t fr1() const noexcept
            {
                return valid() ? _filters.instance->sFilterRegister[_index].FR1 : 0U;
            }

            [[nodiscard]] uint32_t fr2() const noexcept
            {
                return valid() ? _filters.instance->sFilterRegister[_index].FR2 : 0U;
            }

            /**
             * Mask + 32-bit: FR1 = id, FR2 = mask.
             * Проверяет текущие mode()/scale(); Dual16 не поддерживается.
             */
            [[nodiscard]] bool setMask(const Frame::Id& id, const Frame::Id& mask) noexcept
            {
                if (!valid() || mode() != FilterMode::Mask || scale() != FilterScale::Single32)
                    return false;
                return write(id.pack(), mask.pack());
            }

            [[nodiscard]] bool getMask(Frame::Id& id, Frame::Id& mask) const noexcept
            {
                if (!valid() || mode() != FilterMode::Mask || scale() != FilterScale::Single32)
                    return false;
                id = Frame::Id::unpack(fr1());
                mask = Frame::Id::unpack(fr2());
                return true;
            }

            /**
             * List + 32-bit: FR1 = id0, FR2 = id1 (два ID).
             * Проверяет текущие mode()/scale(); Dual16 не поддерживается.
             */
            [[nodiscard]] bool setList(const Frame::Id& id0, const Frame::Id& id1) noexcept
            {
                if (!valid() || mode() != FilterMode::List || scale() != FilterScale::Single32)
                    return false;
                return write(id0.pack(), id1.pack());
            }

            [[nodiscard]] bool getList(Frame::Id& id0, Frame::Id& id1) const noexcept
            {
                if (!valid() || mode() != FilterMode::List || scale() != FilterScale::Single32)
                    return false;
                id0 = Frame::Id::unpack(fr1());
                id1 = Frame::Id::unpack(fr2());
                return true;
            }

            [[nodiscard]] bool setMask(const Frame& id, const Frame& mask) noexcept
            {
                return setMask(id.id, mask.id);
            }

            [[nodiscard]] bool setList(const Frame& id0, const Frame& id1) noexcept
            {
                return setList(id0.id, id1.id);
            }

            /**
             * mode/scale/fifo/FR (+ activate). Требует FINIT снаружи:
             * filter.initEnter(); filter[i].configure(…); filter.initLeave();
             */
            [[nodiscard]] bool configure(FilterMode mode, FilterScale scale, FilterFifo fifo,
                                         uint32_t fr1, uint32_t fr2, bool active = true) noexcept
            {
                if (!valid())
                    return false;
                (void)setActive(false);
                (void)setMode(mode);
                (void)setScale(scale);
                (void)setFifo(fifo);
                (void)write(fr1, fr2);
                (void)setActive(active);
                return true;
            }

            /// 32-bit mask, ID/mask = 0 → accept all → FIFO0. Требует FINIT снаружи.
            [[nodiscard]] bool acceptAll() noexcept
            {
                return configure(FilterMode::Mask, FilterScale::Single32, FilterFifo::Fifo0, 0U, 0U, true);
            }
        };

        [[nodiscard]] Bank operator[](uint8_t bank) noexcept { return Bank{*this, bank}; }
    };

    Filter filter;

    CAN()
        : instance(reinterpret_cast<CAN_TypeDef*>(static_cast<uintptr_t>(CanId)))
        , filter(instance)
    {
    }

    /// RX/TX в AF (AF из IBase, на F4 для CAN1 — AF9).
    void InitPins(const GPIO::Pin& rx, const GPIO::Pin& tx) const
    {
        tx.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        rx.Init(GPIO::ModeAlt::PP, this->af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
    }

    /**
     * Init mode → BTR (+ filter[0].acceptAll) → normal mode.
     * Если timing.prescaler == 0 — подобрать forBaud(pclk, baud_hz).
     * Перед вызовом: EnableClock().
     */
    bool Configure(uint32_t baud_hz, BitTiming timing = {}) noexcept
    {
        if (baud_hz == 0U && timing.prescaler == 0U)
            return false;

        if (timing.prescaler == 0U) {
            const uint32_t pclk = this->InputClockHz();
            if (!BitTiming::forBaud(pclk, baud_hz, timing))
                return false;
        }

        if (!enterInitMode())
            return false;

        btr.write(timing.toBtr());
        filter.initEnter();
        (void)filter[0].acceptAll();
        filter.initLeave();

        if (!leaveInitMode())
            return false;
        return true;
    }

    /// Init mode (+ sleep). Прерывания в IER снимает вызывающий.
    void Shutdown() noexcept
    {
        (void)enterInitMode();
        mcr.set(MCR::SLEEP);
    }

    /**
     * Положить кадр в свободный TX mailbox (0…2). false — все заняты / плохой DLC.
     */
    bool tryTransmit(const Frame& frame) noexcept
    {
        if (frame.dlc > BIF::CAN::kMaxDataLength)
            return false;

        uint32_t mb = 0xFFU;
        const REG::BitMask<TSR> st = tsr.read();
        if (st.any(TSR::TME0))
            mb = 0U;
        else if (st.any(TSR::TME1))
            mb = 1U;
        else if (st.any(TSR::TME2))
            mb = 2U;
        else
            return false;

        CAN_TxMailBox_TypeDef& box = instance->sTxMailBox[mb];

        uint32_t tir_v = frame.id.pack();
        box.TDTR = static_cast<uint32_t>(frame.dlc) & CAN_TDT0R_DLC;
        box.TDLR = (static_cast<uint32_t>(frame.data[0]))
                   | (static_cast<uint32_t>(frame.data[1]) << 8)
                   | (static_cast<uint32_t>(frame.data[2]) << 16)
                   | (static_cast<uint32_t>(frame.data[3]) << 24);
        box.TDHR = (static_cast<uint32_t>(frame.data[4]))
                   | (static_cast<uint32_t>(frame.data[5]) << 8)
                   | (static_cast<uint32_t>(frame.data[6]) << 16)
                   | (static_cast<uint32_t>(frame.data[7]) << 24);
        box.TIR = tir_v | CAN_TI0R_TXRQ;
        return true;
    }

    /// Прочитать кадр из FIFO0 и освободить mailbox. false — FIFO пуст.
    bool tryReceive(Frame& out) noexcept
    {
        if (!rf0r.any(RF0R::FMP0))
            return false;

        const CAN_FIFOMailBox_TypeDef& box = instance->sFIFOMailBox[0];
        out.id = Frame::Id::unpack(box.RIR);
        out.dlc = static_cast<uint8_t>(box.RDTR & CAN_RDT0R_DLC);

        const uint32_t rdlr_v = box.RDLR;
        const uint32_t rdhr_v = box.RDHR;
        out.data[0] = static_cast<uint8_t>(rdlr_v);
        out.data[1] = static_cast<uint8_t>(rdlr_v >> 8);
        out.data[2] = static_cast<uint8_t>(rdlr_v >> 16);
        out.data[3] = static_cast<uint8_t>(rdlr_v >> 24);
        out.data[4] = static_cast<uint8_t>(rdhr_v);
        out.data[5] = static_cast<uint8_t>(rdhr_v >> 8);
        out.data[6] = static_cast<uint8_t>(rdhr_v >> 16);
        out.data[7] = static_cast<uint8_t>(rdhr_v >> 24);

        rf0r.set(RF0R::RFOM0);
        return true;
    }

private:
    bool enterInitMode() noexcept
    {
        mcr.clear(MCR::SLEEP);
        if (!waitMsrClear(MSR::SLAK))
            return false;

        mcr.set(MCR::INRQ);
        return waitMsrSet(MSR::INAK);
    }

    bool leaveInitMode() noexcept
    {
        mcr.clear(MCR::INRQ);
        return waitMsrClear(MSR::INAK);
    }

    bool waitMsrSet(MSR bit) noexcept
    {
        for (uint32_t i = 0; i < detail::kModeWaitSpins; ++i) {
            if (msr.any(bit))
                return true;
        }
        return false;
    }

    bool waitMsrClear(MSR bit) noexcept
    {
        for (uint32_t i = 0; i < detail::kModeWaitSpins; ++i) {
            if (!msr.any(bit))
                return true;
        }
        return false;
    }
};

} // namespace CAN
