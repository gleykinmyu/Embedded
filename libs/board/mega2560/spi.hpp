#pragma once

/**
 * SPI master ATmega2560, один модуль, опрос SPIF без прерывания.
 * Пины фиксированы: SS PB0, SCK PB1, MOSI PB2, MISO PB3.
 * SS модуля — выход и держится высоким, иначе низкий вход сбрасывает MSTR.
 * CS устройства — отдельная ножка IDigitalPin, активный низкий уровень.
 */
#include "digital_pin.hpp"
#include "iex_byte_stream.hpp"

#include <avr/io.h>
#include <stddef.h>
#include <stdint.h>

namespace Spi {

struct Regs {
    static inline volatile uint8_t& cr = SPCR;
    static inline volatile uint8_t& sr = SPSR;
    static inline volatile uint8_t& dr = SPDR;
};

enum class Mode : uint8_t { Mode0, Mode1, Mode2, Mode3 };

class Master : public BIF::IExByteStream {
public:
    /** SCK/MOSI/SS — выходы, MISO — вход, SS модуля высокий. */
    void InitPins() const
    {
        DigitalPin<Port::B, 0> ss;
        DigitalPin<Port::B, 1> sck;
        DigitalPin<Port::B, 2> mosi;
        DigitalPin<Port::B, 3> miso;
        ss.Init(BIF::PinMode::Output);
        ss.Set();
        sck.Init(BIF::PinMode::Output);
        mosi.Init(BIF::PinMode::Output);
        miso.Init(BIF::PinMode::Input);
    }

    /** Мягкий CS. Ножка должна жить дольше мастера. До обмена — высокий уровень. */
    void setCs(BIF::IDigitalPin& cs) noexcept
    {
        _cs = &cs;
        cs.Init(BIF::PinMode::Output);
        cs.Set();
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

    /** Только при закрытом порте. Иначе false. */
    bool setMode(Mode mode, bool lsbFirst = false) noexcept
    {
        if (_isOpen)
            return false;
        _mode = mode;
        _lsbFirst = lsbFirst;
        return true;
    }

    bool open(uint32_t baudrate) noexcept
    {
        if (baudrate == 0u)
            return false;

        uint8_t spr = 0;
        bool spi2x = false;
        if (!divider(baudrate, spr, spi2x))
            return false;

        DigitalPin<Port::B, 0>{}.Init(BIF::PinMode::Output);
        DigitalPin<Port::B, 0>{}.Set();

        if (spi2x)
            Regs::sr |= static_cast<uint8_t>(1u << SPI2X);
        else
            Regs::sr &= static_cast<uint8_t>(~(1u << SPI2X));

        uint8_t cr = static_cast<uint8_t>((1u << SPE) | (1u << MSTR) | spr);
        if (_mode == Mode::Mode2 || _mode == Mode::Mode3)
            cr = static_cast<uint8_t>(cr | (1u << CPOL));
        if (_mode == Mode::Mode1 || _mode == Mode::Mode3)
            cr = static_cast<uint8_t>(cr | (1u << CPHA));
        if (_lsbFirst)
            cr = static_cast<uint8_t>(cr | (1u << DORD));
        Regs::cr = cr;

        _isOpen = true;
        clearErrors();
        return true;
    }

    void close() noexcept
    {
        deselect();
        Regs::cr &= static_cast<uint8_t>(~(1u << SPE));
        _isOpen = false;
    }

    [[nodiscard]] bool exchange(const uint8_t* tx, uint8_t* rx, size_t n) noexcept override
    {
        if (!_isOpen || n == 0u)
            return n == 0u;

        for (size_t i = 0; i < n; ++i) {
            const uint8_t out = (tx != nullptr) ? tx[i] : 0xFFu;
            Regs::dr = out;
            while ((Regs::sr & static_cast<uint8_t>(1u << SPIF)) == 0) {
            }
            if ((Regs::sr & static_cast<uint8_t>(1u << WCOL)) != 0)
                _dataError = true;
            const uint8_t in = Regs::dr;
            if (rx != nullptr)
                rx[i] = in;
        }
        return true;
    }

    size_t write(const uint8_t* data, size_t size) override
    {
        if (!_isOpen || data == nullptr || size == 0u)
            return 0;
        return exchange(data, nullptr, size) ? size : 0u;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override
    {
        if (!_isOpen || buffer == nullptr || maxSize == 0u)
            return 0;
        return exchange(nullptr, buffer, maxSize) ? maxSize : 0u;
    }

    size_t available() const override { return 0; }

    size_t availableForWrite() const override
    {
        return _isOpen ? static_cast<size_t>(-1) : 0u;
    }

    void purge() override {}
    void purgeOutput() override {}
    void flush() override {}

    bool isOpen() override { return _isOpen; }

    Status getStatus() override
    {
        if (!_isOpen)
            return Status::OK;
        if (_dataError)
            return Status::DataError;
        return Status::OK;
    }

    void clearErrors() override { _dataError = false; }

private:
    /** Самый быстрый делитель, при котором SCK не выше baudrate. Медленнее /128 нельзя. */
    [[nodiscard]] static bool divider(uint32_t baud, uint8_t& spr, bool& spi2x) noexcept
    {
        struct Opt {
            uint16_t div;
            uint8_t spr;
            bool x2;
        };
        static constexpr Opt kOpt[] = {
            {2, 0u << SPR0, true},  {4, 0u << SPR0, false}, {8, 1u << SPR0, true},
            {16, 1u << SPR0, false}, {32, 1u << SPR1, true}, {64, 1u << SPR1, false},
            {128, (1u << SPR1) | (1u << SPR0), false},
        };
        const uint32_t cpu = static_cast<uint32_t>(F_CPU);
        for (const Opt& opt : kOpt) {
            if ((cpu / opt.div) <= baud) {
                spr = opt.spr;
                spi2x = opt.x2;
                return true;
            }
        }
        spr = kOpt[6].spr;
        spi2x = false;
        return true;
    }

    Mode _mode = Mode::Mode0;
    bool _lsbFirst = false;
    BIF::IDigitalPin* _cs = nullptr;
    bool _isOpen = false;
    bool _dataError = false;
};
} // namespace Spi
