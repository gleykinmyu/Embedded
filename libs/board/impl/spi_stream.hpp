#pragma once
#include "iserial.hpp"
#include "phl/spi.hpp"

namespace PHL {

/**
 * Реализация BIF::ISerial / IByteStream для SPI (master, soft CS).
 *
 * Полный дуплекс: каждый байт write() тактирует MOSI и кладёт MISO в RX-кольцо —
 * ответ slave читается через read() после (или параллельно с) write/flush.
 * Для «чистого» чтения на шину пишут dummy (обычно 0xFF).
 *
 * CS (активный низкий): select() / deselect(); вне транзакции — высокий.
 */
template <PHL::ID SpiId, size_t TxSize = 128, size_t RxSize = 128>
class SpiStream : public BIF::ISerial<TxSize, RxSize>
{
    static_assert(PHL::GetType<SpiId>::value == PHL::Type::SPI, "SpiStream: только PHL::ID с Type::SPI");

    SPI::SPI<SpiId> _spi;
    SPI::Mode _mode = SPI::Mode::Mode0();
    const GPIO::Pin* _cs = nullptr; ///< soft CS; обязан жить дольше (обычно PortX::pin<N>)
    uint32_t _primaskSave = 0;

public:
    /// SCK/MISO/MOSI — AF; cs — soft GPIO Output, idle = high (deselected).
    /// `cs` должен быть lvalue с временем жизни ≥ потока (напр. `GPIO::PortA::pin<15>`).
    void InitPins(const GPIO::Pin& sck, const GPIO::Pin& miso, const GPIO::Pin& mosi,
        const GPIO::Pin& cs) noexcept
    {
        _spi.InitPins(sck, miso, mosi);
        _cs = &cs;
        cs.Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::High);
        deselect();
    }

    /// Только линии SPI без CS (управление снаружи).
    void InitPins(const GPIO::Pin& sck, const GPIO::Pin& miso, const GPIO::Pin& mosi) noexcept
    {
        _spi.InitPins(sck, miso, mosi);
        _cs = nullptr;
    }

    ~SpiStream() { _spi.IRQ.unregister_handler(); }

    void select() noexcept
    {
        if (_cs != nullptr)
            _cs->Clear();
    }

    void deselect() noexcept
    {
        if (_cs != nullptr)
            _cs->Set();
    }

    bool open(uint32_t baudrate) override
    {
        if (baudrate == 0U)
            return false;

        _spi.EnableClock();

        if (!_spi.ConfigureMaster(baudrate, _mode))
            return false;

        _spi.cr2.set(SPI::CR2::RXNEIE);

        _spi.IRQ.handler(this, &SpiStream::spi_irq);
        _spi.IRQ.set_priority(5);
        _spi.IRQ.enable();

        this->_isOpen = true;
        return true;
    }

    /// Только при закрытом порте; иначе false. Вступает в силу при следующем open().
    bool setMode(const SPI::Mode& fmt) noexcept
    {
        if (this->_isOpen)
            return false;
        _mode = fmt;
        return true;
    }

    void close() override
    {
        deselect();

        _spi.IRQ.disable();
        _spi.IRQ.unregister_handler();

        _spi.cr2.clear(SPI::CR2::RXNEIE);
        _spi.cr2.clear(SPI::CR2::TXEIE);

        _spi.Shutdown();

        this->_isOpen = false;
    }

    /// Обработчик линии SPI IRQ (регистрация: IRQ.handler(this, &SpiStream::spi_irq)).
    void spi_irq() noexcept
    {
        using namespace SPI;

        while (_spi.sr.any(SR::RXNE) && _spi.cr2.any(CR2::RXNEIE)) {
            this->IRQ_RX_Handler();
        }
        if (_spi.sr.any(SR::TXE) && _spi.cr2.any(CR2::TXEIE)) {
            this->IRQ_TX_Handler();
        }
        if (_spi.sr.any(SR::OVR)) {
            // Сброс OVR: чтение DR, затем SR (RM).
            (void)_spi.dr.read();
            (void)_spi.sr.read();
            this->_hwOverrunRx = true;
        }
    }

    /// Доступ к низкоуровневому SPI (polling transfer и т.п.).
    SPI::SPI<SpiId>& spi() noexcept { return _spi; }
    const SPI::SPI<SpiId>& spi() const noexcept { return _spi; }

    /**
     * Полный дуплекс, polling. На время обмена SPI TXEIE/RXNEIE отключаются,
     * чтобы IRQ не унёс байт из DR (для flash-транзакций и т.п.).
     * tx == nullptr → на MOSI 0xFF; rx == nullptr → ответ отбрасывается.
     */
    void busTransfer(const uint8_t* tx, uint8_t* rx, size_t n) noexcept
    {
        using namespace SPI;
        _spi.cr2.clear(CR2::TXEIE | CR2::RXNEIE);
        while (_spi.sr.any(SR::RXNE))
            (void)_spi.dr.read();

        for (size_t i = 0; i < n; ++i) {
            const uint8_t out = (tx != nullptr) ? tx[i] : 0xFFU;
            const uint8_t in = _spi.transfer(out);
            if (rx != nullptr)
                rx[i] = in;
        }

        if (this->_isOpen)
            _spi.cr2.set(CR2::RXNEIE);
    }

    uint8_t busTransfer(uint8_t tx) noexcept
    {
        uint8_t rx = 0;
        busTransfer(&tx, &rx, 1);
        return rx;
    }

protected:
    void IRQ_TX_Enable() override { _spi.cr2.set(SPI::CR2::TXEIE); }

    void IRQ_TX_Disable() override { _spi.cr2.clear(SPI::CR2::TXEIE); }

    bool isHardwareTxBusy() const override { return _spi.sr.any(SPI::SR::BSY); }

    uint8_t readHardware() override { return static_cast<uint8_t>(_spi.dr.read() & 0xFFU); }

    void writeHardware(uint8_t data) override { _spi.dr.write(static_cast<uint32_t>(data)); }

    bool checkErrors() override
    {
        using namespace SPI;

        const REG::BitMask<SR> st = _spi.sr.get();
        if (st.any(SR::MODF))
            this->_dataError = true;
        if (st.any(SR::OVR))
            this->_hwOverrunRx = true;

        // OVR/MODF: байт всё равно считываем в IRQ_RX_Handler; в кольцо не кладём при OVR.
        return st.any(SR::OVR | SR::MODF);
    }

    void lock() override
    {
        _primaskSave = __get_PRIMASK();
        __disable_irq();
    }

    void unlock() override { __set_PRIMASK(_primaskSave); }
};

} // namespace PHL
