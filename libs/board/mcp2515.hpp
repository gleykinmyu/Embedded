#pragma once

/**
 * MCP2515 поверх BIF::IExByteStream (SPI master) и отдельного CS.
 * Снаружи это BIF::CAN::ICAN: кадры, open(bitrate), без регистров МК.
 * SPI уже открыт (режим 0). CS активен низким уровнем.
 * Кварц MCP2515 задаётся в конструкторе (часто 8 или 16 МГц).
 */
#include "ican.hpp"
#include "idigital_pin.hpp"
#include "iex_byte_stream.hpp"

namespace MCP {

class Mcp2515 : public BIF::CAN::ICAN {
    using Frame = BIF::CAN::Frame;
    using Status = BIF::CAN::Status;

public:
    Mcp2515(BIF::IExByteStream& spi, BIF::IDigitalPin& cs, uint32_t oscHz) noexcept
        : _spi(spi)
        , _cs(cs)
        , _oscHz(oscHz)
    {
    }

    bool open(uint32_t bitrate) override
    {
        if (bitrate == 0u || _oscHz == 0u || !_spi.isOpen())
            return false;

        _cs.Init(BIF::PinMode::Output);
        _cs.Set();

        if (!reset() || !setMode(kModeConfig))
            return false;
        if (!writeBitrate(bitrate))
            return false;

        // Принимать все кадры, оба буфера.
        if (!writeReg(kRxb0Ctrl, 0x60u) || !writeReg(kRxb1Ctrl, 0x60u))
            return false;
        if (!writeReg(kCaninte, 0x03u))
            return false;
        if (!setMode(kModeNormal))
            return false;

        _isOpen = true;
        clearErrors();
        return true;
    }

    void close() override
    {
        if (_isOpen)
            setMode(kModeSleep);
        _isOpen = false;
    }

    bool isOpen() override { return _isOpen; }

    bool send(const Frame& frame) override
    {
        if (!_isOpen || frame.dlc > kMaxData)
            return false;

        const uint8_t status = readStatus();
        uint8_t box = 0xFFu;
        if ((status & 0x04u) == 0u)
            box = 0;
        else if ((status & 0x10u) == 0u)
            box = 1;
        else if ((status & 0x40u) == 0u)
            box = 2;
        if (box == 0xFFu)
            return false;

        uint8_t raw[kHeader + kMaxData] = {};
        pack(frame, raw);

        const uint8_t sidh = static_cast<uint8_t>(kTxb0Sidh + box * 0x10u);
        if (!writeRegs(sidh, raw, static_cast<uint8_t>(kHeader + frame.dlc)))
            return false;

        const uint8_t rts = static_cast<uint8_t>(0x80u | (1u << box));
        return command(&rts, 1u);
    }

    bool recv(Frame& out) override
    {
        if (!_isOpen)
            return false;

        const uint8_t status = readStatus();
        uint8_t instr = 0;
        if ((status & 0x01u) != 0u)
            instr = 0x90u;
        else if ((status & 0x02u) != 0u)
            instr = 0x94u;
        else
            return false;

        uint8_t tx[1u + kHeader + kMaxData] = {};
        uint8_t rx[1u + kHeader + kMaxData] = {};
        tx[0] = instr;
        if (!transfer(tx, rx, sizeof(tx)))
            return false;
        unpack(&rx[1], out);
        return true;
    }

    size_t available() const override
    {
        if (!_isOpen)
            return 0;
        const uint8_t status = readStatus();
        size_t n = 0;
        if ((status & 0x01u) != 0u)
            ++n;
        if ((status & 0x02u) != 0u)
            ++n;
        return n;
    }

    size_t availableForWrite() const override
    {
        if (!_isOpen)
            return 0;
        const uint8_t status = readStatus();
        size_t n = 3;
        if ((status & 0x04u) != 0u)
            --n;
        if ((status & 0x10u) != 0u)
            --n;
        if ((status & 0x40u) != 0u)
            --n;
        return n;
    }

    void purge() override
    {
        if (!_isOpen)
            return;
        (void)bitModify(kCanintf, 0x03u, 0u);
        (void)bitModify(kEflg, 0xC0u, 0u);
    }

    void purgeOutput() override
    {
        if (!_isOpen)
            return;
        (void)bitModify(kCanctrl, 0x10u, 0x10u);
        (void)bitModify(kCanctrl, 0x10u, 0u);
    }

    void flush() override
    {
        if (!_isOpen)
            return;
        for (uint16_t i = 0; i < 10000u; ++i) {
            if ((readStatus() & 0x54u) == 0u)
                return;
        }
    }

    Status getStatus() override
    {
        if (!_isOpen)
            return Status::OK;
        if (_status != Status::OK)
            return _status;
        const uint8_t eflg = readReg(kEflg);
        if ((eflg & 0x20u) != 0u)
            return Status::BusOff;
        if ((eflg & 0x18u) != 0u)
            return Status::ErrorPassive;
        if ((eflg & 0x07u) != 0u)
            return Status::ErrorWarning;
        if ((eflg & 0xC0u) != 0u)
            return Status::OverFlowRX;
        return Status::OK;
    }

    void clearErrors() override
    {
        _status = Status::OK;
        if (_isOpen)
            (void)bitModify(kEflg, 0xC0u, 0u);
    }

private:
    static constexpr uint8_t kMaxData = BIF::CAN::kMaxDataLength;
    static constexpr uint8_t kHeader = 5;

    static constexpr uint8_t kCanctrl = 0x0Fu;
    static constexpr uint8_t kCanstat = 0x0Eu;
    static constexpr uint8_t kCanintf = 0x2Cu;
    static constexpr uint8_t kCaninte = 0x2Bu;
    static constexpr uint8_t kEflg = 0x2Du;
    static constexpr uint8_t kCnf3 = 0x28u;
    static constexpr uint8_t kCnf2 = 0x29u;
    static constexpr uint8_t kCnf1 = 0x2Au;
    static constexpr uint8_t kRxb0Ctrl = 0x60u;
    static constexpr uint8_t kRxb1Ctrl = 0x70u;
    static constexpr uint8_t kTxb0Sidh = 0x31u;

    static constexpr uint8_t kModeNormal = 0x00u;
    static constexpr uint8_t kModeSleep = 0x20u;
    static constexpr uint8_t kModeConfig = 0x80u;

    bool reset() noexcept
    {
        const uint8_t cmd = 0xC0u;
        return command(&cmd, 1u);
    }

    bool setMode(uint8_t mode) noexcept
    {
        if (!bitModify(kCanctrl, 0xE0u, mode))
            return false;
        for (uint8_t i = 0; i < 100u; ++i) {
            if ((readReg(kCanstat) & 0xE0u) == mode)
                return true;
        }
        return false;
    }

    bool writeBitrate(uint32_t bitrate) noexcept
    {
        const uint32_t twice = bitrate * 2u;
        if (twice == 0u || (_oscHz % twice) != 0u)
            return false;
        const uint32_t tqProduct = _oscHz / twice;

        uint8_t nbt = 0;
        uint8_t brp = 0;
        uint8_t prseg = 0;
        uint8_t ps1 = 0;
        uint8_t ps2 = 0;
        if (!chooseTiming(tqProduct, nbt, brp, prseg, ps1, ps2))
            return false;
        (void)nbt;

        const uint8_t cnf1 = brp;
        const uint8_t cnf2 = static_cast<uint8_t>(0x80u | ((ps1 - 1u) << 3) | (prseg - 1u));
        const uint8_t cnf3 = static_cast<uint8_t>(ps2 - 1u);
        return writeReg(kCnf3, cnf3) && writeReg(kCnf2, cnf2) && writeReg(kCnf1, cnf1);
    }

    [[nodiscard]] static bool chooseTiming(uint32_t tqProduct, uint8_t& nbt, uint8_t& brp,
                                           uint8_t& prseg, uint8_t& ps1, uint8_t& ps2) noexcept
    {
        for (uint8_t n = 16; n >= 8; --n) {
            if (tryTiming(tqProduct, n, nbt, brp, prseg, ps1, ps2))
                return true;
        }
        for (uint8_t n = 17; n <= 25; ++n) {
            if (tryTiming(tqProduct, n, nbt, brp, prseg, ps1, ps2))
                return true;
        }
        return false;
    }

    [[nodiscard]] static bool tryTiming(uint32_t tqProduct, uint8_t n, uint8_t& nbt, uint8_t& brp,
                                        uint8_t& prseg, uint8_t& ps1, uint8_t& ps2) noexcept
    {
        if (n == 0u || (tqProduct % n) != 0u)
            return false;
        const uint32_t brp1 = tqProduct / n;
        if (brp1 < 1u || brp1 > 64u)
            return false;
        for (uint8_t seg2 = 2; seg2 <= 8; ++seg2) {
            if (seg2 + 1u >= n)
                break;
            const uint8_t left = static_cast<uint8_t>(n - 1u - seg2);
            if (left < 2u || left > 16u)
                continue;
            const uint8_t prop = static_cast<uint8_t>(left / 2u);
            const uint8_t phase1 = static_cast<uint8_t>(left - prop);
            if (prop < 1u || prop > 8u || phase1 < 1u || phase1 > 8u)
                continue;
            nbt = n;
            brp = static_cast<uint8_t>(brp1 - 1u);
            prseg = prop;
            ps1 = phase1;
            ps2 = seg2;
            return true;
        }
        return false;
    }

    static void pack(const Frame& frame, uint8_t* raw) noexcept
    {
        const uint32_t id = frame.id.get();
        if (frame.id.extended) {
            raw[0] = static_cast<uint8_t>((id >> 21) & 0xFFu);
            raw[1] = static_cast<uint8_t>((((id >> 18) & 0x07u) << 5) | 0x08u | ((id >> 16) & 0x03u));
            raw[2] = static_cast<uint8_t>((id >> 8) & 0xFFu);
            raw[3] = static_cast<uint8_t>(id & 0xFFu);
        } else {
            raw[0] = static_cast<uint8_t>((id >> 3) & 0xFFu);
            raw[1] = static_cast<uint8_t>((id & 0x07u) << 5);
            raw[2] = 0;
            raw[3] = 0;
        }
        raw[4] = static_cast<uint8_t>(frame.dlc & 0x0Fu);
        if (frame.id.remote)
            raw[4] = static_cast<uint8_t>(raw[4] | 0x40u);
        const uint8_t n = frame.id.remote ? 0u : frame.dlc;
        for (uint8_t i = 0; i < n; ++i)
            raw[5u + i] = frame.data[i];
    }

    static void unpack(const uint8_t* raw, Frame& out) noexcept
    {
        const bool extended = (raw[1] & 0x08u) != 0u;
        uint32_t id = 0;
        if (extended) {
            id = (static_cast<uint32_t>(raw[0]) << 21) |
                 (static_cast<uint32_t>((raw[1] >> 5) & 0x07u) << 18) |
                 (static_cast<uint32_t>(raw[1] & 0x03u) << 16) |
                 (static_cast<uint32_t>(raw[2]) << 8) | raw[3];
        } else {
            id = (static_cast<uint32_t>(raw[0]) << 3) | ((raw[1] >> 5) & 0x07u);
        }
        out.id = Frame::Id(id, extended, (raw[4] & 0x40u) != 0u);
        out.dlc = static_cast<uint8_t>(raw[4] & 0x0Fu);
        if (out.dlc > kMaxData)
            out.dlc = kMaxData;
        const uint8_t n = out.id.remote ? 0u : out.dlc;
        for (uint8_t i = 0; i < n; ++i)
            out.data[i] = raw[5u + i];
    }

    [[nodiscard]] bool command(const uint8_t* tx, uint8_t n) noexcept
    {
        return transfer(tx, nullptr, n);
    }

    [[nodiscard]] uint8_t readReg(uint8_t addr) noexcept
    {
        uint8_t tx[3] = {0x03u, addr, 0xFFu};
        uint8_t rx[3] = {};
        if (!transfer(tx, rx, 3u))
            return 0;
        return rx[2];
    }

    [[nodiscard]] bool writeReg(uint8_t addr, uint8_t value) noexcept
    {
        uint8_t tx[3] = {0x02u, addr, value};
        return command(tx, 3u);
    }

    [[nodiscard]] bool writeRegs(uint8_t addr, const uint8_t* data, uint8_t n) noexcept
    {
        uint8_t tx[2u + kHeader + kMaxData] = {0x02u, addr};
        for (uint8_t i = 0; i < n; ++i)
            tx[2u + i] = data[i];
        return command(tx, static_cast<uint8_t>(2u + n));
    }

    [[nodiscard]] bool bitModify(uint8_t addr, uint8_t mask, uint8_t value) noexcept
    {
        uint8_t tx[4] = {0x05u, addr, mask, value};
        return command(tx, 4u);
    }

    [[nodiscard]] uint8_t readStatus() const noexcept
    {
        uint8_t tx[2] = {0xA0u, 0xFFu};
        uint8_t rx[2] = {};
        if (!transfer(tx, rx, 2u))
            return 0;
        return rx[1];
    }

    [[nodiscard]] bool transfer(const uint8_t* tx, uint8_t* rx, size_t n) const noexcept
    {
        _cs.Clear();
        const bool ok = _spi.exchange(tx, rx, n);
        _cs.Set();
        return ok;
    }

    BIF::IExByteStream& _spi;
    BIF::IDigitalPin& _cs;
    uint32_t _oscHz;
    bool _isOpen = false;
    Status _status = Status::OK;
};

} // namespace MCP
