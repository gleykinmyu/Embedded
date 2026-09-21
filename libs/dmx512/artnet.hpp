/**
 * @file artnet.hpp
 * @brief Art-Net 4 OpDmx (0x5000) поверх IUdp. Порт 6454.
 */
#pragma once

#include "idmx.hpp"
#include "iudp.hpp"
#include "netutil.hpp"

#include <cstring>

namespace dmx {

inline constexpr uint16_t kArtNetPort = 6454u;
inline constexpr uint16_t kArtOpDmx = 0x5000u;
inline constexpr uint16_t kArtProtVer = 14u;
inline constexpr std::size_t kArtDmxHdr = 18u;
inline constexpr std::size_t kArtDmxMax = kArtDmxHdr + kMaxChannels;

class ArtNetPort : public IDmx {
public:
    struct Config {
        uint16_t universe = 0; ///< packArtNet(net, subnet, uni)
        BIF::NET::Ipv4 destIp = 0xFFFFFFFFu; ///< broadcast по умолчанию
        uint16_t destPort = kArtNetPort;
        uint8_t physical = 0;
    };

    explicit ArtNetPort(BIF::NET::IUdp& udp, const Config& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
    }

    void setConfig(const Config& cfg) noexcept { _cfg = cfg; }
    [[nodiscard]] const Config& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(kArtNetPort))
            return false;
        _seq = 1;
        _isOpen = true;
        _frameReady = false;
        return true;
    }

    void close() override
    {
        _udp.close();
        _isOpen = false;
        _frameReady = false;
    }

    [[nodiscard]] bool isOpen() const override { return _isOpen; }
    [[nodiscard]] Transport transport() const override { return Transport::ArtNet; }
    [[nodiscard]] Direction direction() const override { return _dir; }

    bool setDirection(Direction dir) override
    {
        _dir = dir;
        _frameReady = false;
        return true;
    }

    [[nodiscard]] uint16_t universe() const override { return _cfg.universe; }
    void setUniverse(uint16_t universe) override { _cfg.universe = universe & 0x7FFFu; }

    bool send(const Frame& frame) override
    {
        if (!_isOpen || _dir != Direction::Transmit)
            return false;

        uint8_t pkt[kArtDmxMax]{};
        const std::size_t n = encode(pkt, frame);
        if (n == 0u)
            return false;

        BIF::NET::Endpoint dest{_cfg.destIp, _cfg.destPort};
        if (!_udp.sendTo(pkt, n, dest))
            return false;
        ++_frameCount;
        return true;
    }

    bool recv(Frame& frame) override
    {
        if (!_frameReady)
            return false;
        frame = _rx;
        _frameReady = false;
        return true;
    }

    void poll() override
    {
        if (!_isOpen || _dir != Direction::Receive)
            return;

        uint8_t pkt[kArtDmxMax];
        BIF::NET::Endpoint src{};
        for (;;) {
            const std::size_t n = _udp.recvFrom(pkt, sizeof(pkt), src);
            if (n == 0u)
                break;
            Frame parsed{};
            if (!decode(pkt, n, parsed)) {
                _status = Status::DataError;
                continue;
            }
            _rx = parsed;
            _frameReady = true;
            ++_frameCount;
        }
    }

    [[nodiscard]] std::size_t available() const override { return _frameReady ? 1u : 0u; }

    void purge() override { _frameReady = false; }

    Status getStatus() override { return _status; }

    void clearErrors() override { _status = Status::OK; }

    [[nodiscard]] uint32_t frameCount() const override { return _frameCount; }

private:
    static constexpr char kId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};

    std::size_t encode(uint8_t* pkt, const Frame& frame) noexcept
    {
        std::size_t n = frame.count;
        if (n > kMaxChannels)
            n = kMaxChannels;
        if (n < 2u)
            n = 2u;
        if ((n & 1u) != 0u)
            ++n;

        std::memcpy(pkt, kId, 8u);
        net::putLe16(pkt + 8, kArtOpDmx);
        pkt[10] = 0;
        pkt[11] = static_cast<uint8_t>(kArtProtVer);
        pkt[12] = _seq;
        if (_seq == 255u)
            _seq = 1;
        else
            ++_seq;
        pkt[13] = _cfg.physical;
        pkt[14] = static_cast<uint8_t>(_cfg.universe);
        pkt[15] = static_cast<uint8_t>(_cfg.universe >> 8);
        net::putBe16(pkt + 16, static_cast<uint16_t>(n));
        std::memcpy(pkt + kArtDmxHdr, frame.slots, frame.count);
        if (n > frame.count)
            std::memset(pkt + kArtDmxHdr + frame.count, 0, n - frame.count);
        (void)frame.startCode; ///< ArtDmx payload — только слоты, SC=0.
        return kArtDmxHdr + n;
    }

    bool decode(const uint8_t* pkt, std::size_t n, Frame& out) noexcept
    {
        if (n < kArtDmxHdr + 2u)
            return false;
        if (std::memcmp(pkt, kId, 8u) != 0)
            return false;
        if (net::le16(pkt + 8) != kArtOpDmx)
            return false;
        if (pkt[11] < 14u)
            return false;

        const uint16_t uni = static_cast<uint16_t>(pkt[14] | (static_cast<uint16_t>(pkt[15]) << 8));
        if (uni != (_cfg.universe & 0x7FFFu))
            return false;

        uint16_t len = net::be16(pkt + 16);
        if (len < 2u || (len & 1u) != 0u)
            return false;
        if (len > kMaxChannels)
            len = static_cast<uint16_t>(kMaxChannels);
        if (n < kArtDmxHdr + len)
            return false;

        out.startCode = 0;
        out.count = len;
        std::memcpy(out.slots, pkt + kArtDmxHdr, len);
        if (len < kMaxChannels)
            std::memset(out.slots + len, 0, kMaxChannels - len);
        return true;
    }

    BIF::NET::IUdp& _udp;
    Config _cfg;
    Frame _rx{};
    Direction _dir = Direction::Receive;
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    uint8_t _seq = 1;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
