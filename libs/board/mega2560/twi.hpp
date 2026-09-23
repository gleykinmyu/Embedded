#pragma once

/**
 * TWI master (I2C), пины Mega: SCL PD0 (D21), SDA PD1 (D20).
 * Нужны внешние подтяжки. Внутренние включаются как слабый запас.
 * Ожидание TWINT ограничено по BoardClock::millis (тик уже должен идти).
 */
#include "clock.hpp"
#include "gpio.hpp"

#include <stdint.h>

namespace PHL {

class TwiMaster {
public:
    [[nodiscard]] bool open(uint32_t freq_hz) noexcept
    {
        if (freq_hz == 0u)
            return false;

        const uint32_t cpu = static_cast<uint32_t>(F_CPU);
        if (cpu / freq_hz < 16u)
            return false;
        const uint32_t twbr = (cpu / freq_hz - 16u) / 2u;
        if (twbr > 255u)
            return false;

        GPIO::PortD::pin<0>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        GPIO::PortD::pin<1>.Init(GPIO::Mode::Input, GPIO::Pull::Up);

        TWSR = static_cast<uint8_t>(TWSR & static_cast<uint8_t>(~((1u << TWPS1) | (1u << TWPS0))));
        TWBR = static_cast<uint8_t>(twbr);
        TWCR = static_cast<uint8_t>(1u << TWEN);
        _open = true;
        return true;
    }

    void close() noexcept
    {
        TWCR = 0;
        _open = false;
    }

    [[nodiscard]] bool isOpen() const noexcept { return _open; }

    [[nodiscard]] bool writeTo(uint8_t addr7, const uint8_t* data, uint8_t len) noexcept
    {
        if (!_open || data == nullptr)
            return false;
        if (!start())
            return false;
        if (!sla(static_cast<uint8_t>(addr7 << 1), 0x18u)) {
            stop();
            return false;
        }
        for (uint8_t i = 0; i < len; ++i) {
            if (!writeByte(data[i])) {
                stop();
                return false;
            }
        }
        stop();
        return true;
    }

    [[nodiscard]] bool readFrom(uint8_t addr7, uint8_t* data, uint8_t len) noexcept
    {
        if (!_open || data == nullptr || len == 0u)
            return false;
        if (!start())
            return false;
        if (!sla(static_cast<uint8_t>((addr7 << 1) | 1u), 0x40u)) {
            stop();
            return false;
        }
        for (uint8_t i = 0; i < len; ++i) {
            const bool ack = static_cast<uint8_t>(i + 1u) < len;
            if (!readByte(data[i], ack)) {
                stop();
                return false;
            }
        }
        stop();
        return true;
    }

private:
    bool _open = false;

    [[nodiscard]] static uint8_t status() noexcept
    {
        return static_cast<uint8_t>(TWSR & 0xF8u);
    }

    [[nodiscard]] bool waitTwint() noexcept
    {
        const uint32_t t0 = BoardClock::millis();
        while ((TWCR & static_cast<uint8_t>(1u << TWINT)) == 0) {
            if ((BoardClock::millis() - t0) > 20u)
                return false;
        }
        return true;
    }

    [[nodiscard]] bool start() noexcept
    {
        TWCR = static_cast<uint8_t>((1u << TWINT) | (1u << TWSTA) | (1u << TWEN));
        if (!waitTwint())
            return false;
        const uint8_t st = status();
        return st == 0x08u || st == 0x10u;
    }

    void stop() noexcept
    {
        TWCR = static_cast<uint8_t>((1u << TWINT) | (1u << TWSTO) | (1u << TWEN));
    }

    [[nodiscard]] bool sla(uint8_t addr8, uint8_t expect) noexcept
    {
        TWDR = addr8;
        TWCR = static_cast<uint8_t>((1u << TWINT) | (1u << TWEN));
        if (!waitTwint())
            return false;
        return status() == expect;
    }

    [[nodiscard]] bool writeByte(uint8_t data) noexcept
    {
        TWDR = data;
        TWCR = static_cast<uint8_t>((1u << TWINT) | (1u << TWEN));
        if (!waitTwint())
            return false;
        return status() == 0x28u;
    }

    [[nodiscard]] bool readByte(uint8_t& data, bool ack) noexcept
    {
        uint8_t cr = static_cast<uint8_t>((1u << TWINT) | (1u << TWEN));
        if (ack)
            cr = static_cast<uint8_t>(cr | (1u << TWEA));
        TWCR = cr;
        if (!waitTwint())
            return false;
        data = TWDR;
        return status() == (ack ? 0x50u : 0x58u);
    }
};

} // namespace PHL
