#pragma once
#include <stddef.h>
#include "iex_byte_stream.hpp"
#include "phl/spi.hpp"

namespace PHL {

/**
 * Базовый SPI master (polling, без IRQ и без кольцевых буферов).
 *
 * Реализует BIF::IExByteStream: основной API — `exchange()`.
 * `write`/`read` — обёртки над exchange (MISO отбрасывается / на MOSI 0xFF).
 * CS (активный низкий): select() / deselect(); вне транзакции — высокий.
 */
template <PHL::ID SpiId>
class SpiStream : public BIF::IExByteStream
{
    static_assert(PHL::GetType<SpiId>::value == PHL::Type::SPI, "SpiStream: только PHL::ID с Type::SPI");

    SPI::SPI<SpiId> _spi;
    SPI::Mode _mode = SPI::Mode::Mode0();
    const GPIO::Pin* _cs = nullptr; ///< soft CS; обязан жить дольше (обычно PortX::pin<N>)
    bool _isOpen = false;
    bool _dataError = false;
    bool _hwOverrunRx = false;

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

    bool open(uint32_t baudrate) noexcept
    {
        if (baudrate == 0U)
            return false;

        _spi.EnableClock();

        if (!_spi.ConfigureMaster(baudrate, _mode))
            return false;

        // Без IRQ: RXNEIE/TXEIE/ERRIE не включаем.
        _spi.cr2.clear(SPI::CR2::RXNEIE | SPI::CR2::TXEIE | SPI::CR2::ERRIE);

        drainRx();
        _isOpen = true;
        clearErrors();
        return true;
    }

    /// Только при закрытом порте; иначе false. Вступает в силу при следующем open().
    bool setMode(const SPI::Mode& fmt) noexcept
    {
        if (_isOpen)
            return false;
        _mode = fmt;
        return true;
    }

    void close() noexcept
    {
        deselect();
        _spi.Shutdown();
        _isOpen = false;
    }

    SPI::SPI<SpiId>& spi() noexcept { return _spi; }
    const SPI::SPI<SpiId>& spi() const noexcept { return _spi; }

    [[nodiscard]] bool exchange(const uint8_t* tx, uint8_t* rx, size_t n) noexcept override
    {
        if (!_isOpen || n == 0U)
            return n == 0U;

        using namespace SPI;
        drainRx();

        for (size_t i = 0; i < n; ++i) {
            const uint8_t out = (tx != nullptr) ? tx[i] : 0xFFU;
            const uint8_t in = _spi.transfer(out);
            if (rx != nullptr)
                rx[i] = in;
        }

        noteSrErrors();
        return true;
    }

    size_t write(const uint8_t* data, size_t size) override
    {
        if (!_isOpen || data == nullptr || size == 0U)
            return 0;
        return exchange(data, nullptr, size) ? size : 0U;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override
    {
        if (!_isOpen || buffer == nullptr || maxSize == 0U)
            return 0;
        return exchange(nullptr, buffer, maxSize) ? maxSize : 0U;
    }

    size_t available() const override { return 0; }

    size_t availableForWrite() const override { return _isOpen ? static_cast<size_t>(-1) : 0U; }

    void purge() override
    {
        if (_isOpen)
            drainRx();
    }

    void purgeOutput() override {}

    void flush() override
    {
        if (!_isOpen)
            return;
        while (_spi.sr.any(SPI::SR::BSY)) {
        }
    }

    bool isOpen() override { return _isOpen; }

    Status getStatus() override
    {
        if (!_isOpen)
            return Status::OK;
        if (_hwOverrunRx)
            return Status::OverFlowRX;
        if (_dataError)
            return Status::DataError;
        return Status::OK;
    }

    void clearErrors() override
    {
        _dataError = false;
        _hwOverrunRx = false;
    }

private:
    void drainRx() noexcept
    {
        using namespace SPI;
        while (_spi.sr.any(SR::RXNE))
            (void)_spi.dr.read();
        // OVR: чтение DR, затем SR (RM).
        if (_spi.sr.any(SR::OVR)) {
            (void)_spi.dr.read();
            (void)_spi.sr.read();
            _hwOverrunRx = true;
        }
    }

    void noteSrErrors() noexcept
    {
        using namespace SPI;
        const REG::BitMask<SR> st = _spi.sr.get();
        if (st.any(SR::MODF))
            _dataError = true;
        if (st.any(SR::OVR)) {
            (void)_spi.dr.read();
            (void)_spi.sr.read();
            _hwOverrunRx = true;
        }
    }
};

} // namespace PHL
