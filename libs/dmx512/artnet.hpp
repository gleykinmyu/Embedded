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

using Frame = BIF::dmx::Frame;
using Status = BIF::dmx::Status;
using iTx = BIF::dmx::iTx;
using iRx = BIF::dmx::iRx;

inline constexpr uint16_t kArtNetPort = 6454u;
inline constexpr uint16_t kArtOpDmx = 0x5000u;
inline constexpr uint16_t kArtProtVer = 14u;
inline constexpr std::size_t kArtDmxHdr = 18u;
inline constexpr std::size_t kArtDmxMax = kArtDmxHdr + BIF::dmx::kMaxChannels;

struct ArtNetConfig {
    uint16_t universe = 0; ///< packArtNet(net, subnet, uni)
    BIF::NET::Ipv4 destIp = 0xFFFFFFFFu; ///< broadcast по умолчанию
    uint16_t destPort = kArtNetPort;
    uint8_t physical = 0;
};

namespace artnet_detail {

inline constexpr char kId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};

inline std::size_t encode(uint8_t* pkt, const Frame& frame, const ArtNetConfig& cfg, uint8_t& seq) noexcept
{
    std::size_t n = BIF::dmx::kMaxChannels;
    if ((n & 1u) != 0u)
        ++n;

    std::memcpy(pkt, kId, 8u);
    net::putLe16(pkt + 8, kArtOpDmx);
    pkt[10] = 0;
    pkt[11] = static_cast<uint8_t>(kArtProtVer);
    pkt[12] = seq;
    if (seq == 255u)
        seq = 1;
    else
        ++seq;
    pkt[13] = cfg.physical;
    pkt[14] = static_cast<uint8_t>(cfg.universe);
    pkt[15] = static_cast<uint8_t>(cfg.universe >> 8);
    net::putBe16(pkt + 16, static_cast<uint16_t>(n));
    std::memcpy(pkt + kArtDmxHdr, frame.slots, BIF::dmx::kMaxChannels);
    if (n > BIF::dmx::kMaxChannels)
        std::memset(pkt + kArtDmxHdr + BIF::dmx::kMaxChannels, 0, n - BIF::dmx::kMaxChannels);
    return kArtDmxHdr + n;
}

inline bool decode(const uint8_t* pkt, std::size_t n, Frame& out, uint16_t expectUni) noexcept
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
    if (uni != (expectUni & 0x7FFFu))
        return false;

    uint16_t len = net::be16(pkt + 16);
    if (len < 2u || (len & 1u) != 0u)
        return false;
    if (len > BIF::dmx::kMaxChannels)
        len = static_cast<uint16_t>(BIF::dmx::kMaxChannels);
    if (n < kArtDmxHdr + len)
        return false;

    std::memcpy(out.slots, pkt + kArtDmxHdr, len);
    if (len < BIF::dmx::kMaxChannels)
        std::memset(out.slots + len, 0, BIF::dmx::kMaxChannels - len);
    return true;
}

} // namespace artnet_detail

class ArtNetTx : public iTx {
public:
    explicit ArtNetTx(BIF::NET::IUdp& udp, const ArtNetConfig& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
    }

    void setConfig(const ArtNetConfig& cfg) noexcept { _cfg = cfg; }
    [[nodiscard]] const ArtNetConfig& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(kArtNetPort))
            return false;
        _seq = 1;
        _isOpen = true;
        return true;
    }

    void close() override
    {
        _udp.close();
        _isOpen = false;
    }

    [[nodiscard]] bool isOpen() const override { return _isOpen; }

    bool send(const Frame& frame) override
    {
        if (!_isOpen)
            return false;

        uint8_t pkt[kArtDmxMax]{};
        const std::size_t n = artnet_detail::encode(pkt, frame, _cfg, _seq);
        BIF::NET::Endpoint dest{_cfg.destIp, _cfg.destPort};
        if (!_udp.sendTo(pkt, n, dest))
            return false;
        ++_frameCount;
        return true;
    }

    Status getStatus() override { return _status; }
    void clearErrors() override { _status = Status::OK; }
    [[nodiscard]] uint32_t frameCount() const override { return _frameCount; }

private:
    BIF::NET::IUdp& _udp;
    ArtNetConfig _cfg;
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    uint8_t _seq = 1;
    bool _isOpen = false;
};

class ArtNetRx : public iRx {
public:
    explicit ArtNetRx(BIF::NET::IUdp& udp, const ArtNetConfig& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
    }

    void setConfig(const ArtNetConfig& cfg) noexcept { _cfg = cfg; }
    [[nodiscard]] const ArtNetConfig& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(kArtNetPort))
            return false;
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
        if (!_isOpen)
            return;

        uint8_t pkt[kArtDmxMax];
        BIF::NET::Endpoint src{};
        for (;;) {
            const std::size_t n = _udp.recvFrom(pkt, sizeof(pkt), src);
            if (n == 0u)
                break;
            Frame parsed{};
            if (!artnet_detail::decode(pkt, n, parsed, _cfg.universe)) {
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
    BIF::NET::IUdp& _udp;
    ArtNetConfig _cfg;
    Frame _rx{};
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
