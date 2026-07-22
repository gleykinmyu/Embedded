#pragma once
#include "ican.hpp"
#include "phl/can.hpp"
#include "core/critical_section.hpp"

namespace PHL {

/**
 * Реализация BIF::CAN::BufferedCAN / IHardwareCAN на bxCAN (PHL::ID).
 * InitPins → open(bitrate); фильтр bank0 accept-all внутри Configure.
 * IRQ: TX / RX0 / RX1 / SCE → BufferedCAN::IRQ_*_Handler.
 */
template <PHL::ID CanId, size_t TxFrames = 16, size_t RxFrames = 16>
class CanBus : public BIF::CAN::BufferedCAN<TxFrames, RxFrames>
{
    static_assert(PHL::GetType<CanId>::value == PHL::Type::CAN, "CanBus: только PHL::ID с Type::CAN");

    using Irq = typename ::CAN::CAN<CanId>::IRQ;

    ::CAN::CAN<CanId> _can;
    uint32_t _primaskSave = 0;

public:
    void InitPins(const GPIO::Pin& rx, const GPIO::Pin& tx) const
    {
        _can.InitPins(rx, tx);
    }

    ::CAN::CAN<CanId>& can() noexcept { return _can; }
    const ::CAN::CAN<CanId>& can() const noexcept { return _can; }

    ~CanBus()
    {
        Irq::TX.unregister_handler();
        Irq::RX0.unregister_handler();
        Irq::RX1.unregister_handler();
        Irq::SCE.unregister_handler();
    }

    bool open(uint32_t bitrate) override
    {
        if (bitrate == 0U)
            return false;

        _can.EnableClock();

        if (!_can.Configure(bitrate))
            return false;

        _can.ier.clear(::CAN::IER::TMEIE | ::CAN::IER::FMPIE0 | ::CAN::IER::FMPIE1 |
                       ::CAN::IER::FOVIE0 | ::CAN::IER::ERRIE);
        _can.ier.set(::CAN::IER::FMPIE0 | ::CAN::IER::ERRIE);

        Irq::TX.handler(this, &CanBus::irq_tx);
        Irq::RX0.handler(this, &CanBus::irq_rx0);
        Irq::RX1.handler(this, &CanBus::irq_rx1);
        Irq::SCE.handler(this, &CanBus::irq_sce);

        Irq::TX.set_priority(5);
        Irq::RX0.set_priority(5);
        Irq::RX1.set_priority(5);
        Irq::SCE.set_priority(5);

        Irq::TX.enable();
        Irq::RX0.enable();
        Irq::RX1.enable();
        Irq::SCE.enable();

        this->_isOpen = true;
        this->_status = BIF::CAN::Status::OK;
        this->_hwOverrunRx = false;
        return true;
    }

    void close() override
    {
        Irq::TX.disable();
        Irq::RX0.disable();
        Irq::RX1.disable();
        Irq::SCE.disable();

        Irq::TX.unregister_handler();
        Irq::RX0.unregister_handler();
        Irq::RX1.unregister_handler();
        Irq::SCE.unregister_handler();

        _can.ier.clear(::CAN::IER::TMEIE | ::CAN::IER::FMPIE0 | ::CAN::IER::FMPIE1 |
                       ::CAN::IER::FOVIE0 | ::CAN::IER::ERRIE);

        _can.Shutdown();
        this->_isOpen = false;
    }

    void irq_tx() noexcept
    {
        if (_can.ier.any(::CAN::IER::TMEIE) &&
            _can.tsr.any(::CAN::TSR::TME0 | ::CAN::TSR::TME1 | ::CAN::TSR::TME2)) {
            this->IRQ_TX_Handler();
        }
    }

    void irq_rx0() noexcept
    {
        if (_can.rf0r.any(::CAN::RF0R::FOVR0)) {
            this->_hwOverrunRx = true;
            _can.rf0r.set(::CAN::RF0R::FOVR0);
        }
        if (_can.ier.any(::CAN::IER::FMPIE0) && _can.rf0r.any(::CAN::RF0R::FMP0)) {
            this->IRQ_RX0_Handler();
        }
    }

    void irq_rx1() noexcept { this->IRQ_RX1_Handler(); }

    void irq_sce() noexcept { this->IRQ_SCE_Handler(); }

protected:
    void IRQ_TX_Enable() override { _can.ier.set(::CAN::IER::TMEIE); }

    void IRQ_TX_Disable() override { _can.ier.clear(::CAN::IER::TMEIE); }

    void IRQ_RX_Enable() override { _can.ier.set(::CAN::IER::FMPIE0); }

    void IRQ_RX_Disable() override { _can.ier.clear(::CAN::IER::FMPIE0); }

    bool tryTransmitHardware(const BIF::CAN::Frame& frame) override
    {
        return _can.tryTransmit(frame);
    }

    bool tryReceiveHardware(BIF::CAN::Frame& out) override
    {
        return _can.tryReceive(out);
    }

    bool isHardwareTxBusy() const override
    {
        // Занято, пока не свободны все три mailbox.
        return !_can.tsr.all(::CAN::TSR::TME0 | ::CAN::TSR::TME1 | ::CAN::TSR::TME2);
    }

    void checkErrors() override
    {
        const REG::BitMask<::CAN::ESR> es = _can.esr.get();
        if (es.any(::CAN::ESR::BOFF))
            this->_status = BIF::CAN::Status::BusOff;
        else if (es.any(::CAN::ESR::EPVF))
            this->_status = BIF::CAN::Status::ErrorPassive;
        else if (es.any(::CAN::ESR::EWGF))
            this->_status = BIF::CAN::Status::ErrorWarning;

        if (_can.msr.any(::CAN::MSR::ERRI))
            _can.msr.set(::CAN::MSR::ERRI);
    }

    void lock() override { _primaskSave = CriticalSection::saveAndDisable(); }

    void unlock() override { CriticalSection::restore(_primaskSave); }
};

} // namespace PHL
