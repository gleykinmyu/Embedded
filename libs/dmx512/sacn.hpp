/**
 * @file sacn.hpp
 * @brief sACN E1.31 DATA packet поверх IUdp. Порт 5568, multicast 239.255.x.y.
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

inline constexpr uint16_t kSacnPort = 5568u;
inline constexpr std::size_t kSacnScOff = 125u;
inline constexpr std::size_t kSacnMax = kSacnScOff + 1u + BIF::dmx::kMaxChannels; // 638

struct SacnConfig {
    uint16_t universe = 1; ///< 1…63999
    uint8_t priority = 100;
    uint8_t cid[16]{};
    char sourceName[64]{};
    bool multicast = true;
    BIF::NET::Ipv4 destIp = 0; ///< unicast, если !multicast
};

namespace sacn_detail {

inline constexpr char kAcnId[12] = {'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', '\0', '\0', '\0'};

inline void clampUniverse(uint16_t& universe) noexcept
{
    if (universe == 0u)
        universe = 1u;
    if (universe > 63999u)
        universe = 63999u;
}

inline void defaultName(SacnConfig& cfg) noexcept
{
    if (cfg.sourceName[0] != '\0')
        return;
    const char* name = "DMX";
    std::size_t i = 0;
    while (name[i] != '\0' && i < 63u) {
        cfg.sourceName[i] = name[i];
        ++i;
    }
}

inline std::size_t encode(uint8_t* pkt, const Frame& frame, const SacnConfig& cfg, uint8_t& seq) noexcept
{
    const uint16_t n = static_cast<uint16_t>(BIF::dmx::kMaxChannels);
    const uint16_t propCount = static_cast<uint16_t>(1u + n);
    const uint16_t dmpPdu = static_cast<uint16_t>(10u + propCount);
    const uint16_t framingPdu = static_cast<uint16_t>(77u + dmpPdu);
    const uint16_t rootPdu = static_cast<uint16_t>(22u + framingPdu);

    pkt[0] = 0x00;
    pkt[1] = 0x10;
    pkt[2] = 0x00;
    pkt[3] = 0x00;
    std::memcpy(pkt + 4, kAcnId, 12u);
    net::putBe16(pkt + 16, static_cast<uint16_t>(0x7000u | rootPdu));
    net::putBe32(pkt + 18, 0x00000004u);
    std::memcpy(pkt + 22, cfg.cid, 16u);

    net::putBe16(pkt + 38, static_cast<uint16_t>(0x7000u | framingPdu));
    net::putBe32(pkt + 40, 0x00000002u);
    std::memcpy(pkt + 44, cfg.sourceName, 64u);
    pkt[108] = cfg.priority;
    pkt[109] = 0;
    pkt[110] = 0;
    pkt[111] = seq++;
    pkt[112] = 0;
    net::putBe16(pkt + 113, cfg.universe);

    net::putBe16(pkt + 115, static_cast<uint16_t>(0x7000u | dmpPdu));
    pkt[117] = 0x02;
    pkt[118] = 0xA1;
    pkt[119] = 0x00;
    pkt[120] = 0x00;
    pkt[121] = 0x00;
    pkt[122] = 0x01;
    net::putBe16(pkt + 123, propCount);
    pkt[125] = 0; ///< start code — транспорт
    std::memcpy(pkt + 126, frame.slots, n);
    return static_cast<std::size_t>(126u + n);
}

inline bool decode(const uint8_t* pkt, std::size_t n, Frame& out, uint16_t expectUni) noexcept
{
    if (n < 126u + 1u)
        return false;
    if (pkt[0] != 0x00 || pkt[1] != 0x10)
        return false;
    if (std::memcmp(pkt + 4, kAcnId, 12u) != 0)
        return false;
    if (net::be32(pkt + 18) != 0x00000004u)
        return false;
    if (net::be32(pkt + 40) != 0x00000002u)
        return false;

    const uint16_t uni = net::be16(pkt + 113);
    if (uni != expectUni)
        return false;

    const uint16_t propCount = net::be16(pkt + 123);
    if (propCount < 2u)
        return false;
    uint16_t slots = static_cast<uint16_t>(propCount - 1u);
    if (slots > BIF::dmx::kMaxChannels)
        slots = static_cast<uint16_t>(BIF::dmx::kMaxChannels);
    if (n < 126u + slots)
        return false;

    // pkt[125] = start code — не в Frame
    std::memcpy(out.slots, pkt + 126, slots);
    if (slots < BIF::dmx::kMaxChannels)
        std::memset(out.slots + slots, 0, BIF::dmx::kMaxChannels - slots);
    return true;
}

} // namespace sacn_detail

class SacnTx : public iTx {
public:
    explicit SacnTx(BIF::NET::IUdp& udp, const SacnConfig& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
        sacn_detail::clampUniverse(_cfg.universe);
        sacn_detail::defaultName(_cfg);
    }

    void setConfig(const SacnConfig& cfg) noexcept
    {
        _cfg = cfg;
        sacn_detail::clampUniverse(_cfg.universe);
        sacn_detail::defaultName(_cfg);
    }

    [[nodiscard]] const SacnConfig& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(kSacnPort))
            return false;
        if (_cfg.multicast)
            _udp.setTtl(1);
        _seq = 0;
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

        uint8_t pkt[kSacnMax]{};
        const std::size_t n = sacn_detail::encode(pkt, frame, _cfg, _seq);
        BIF::NET::Endpoint dest;
        dest.port = kSacnPort;
        dest.ip = _cfg.multicast ? net::sacnGroup(_cfg.universe) : _cfg.destIp;
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
    SacnConfig _cfg;
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    uint8_t _seq = 0;
    bool _isOpen = false;
};

class SacnRx : public iRx {
public:
    explicit SacnRx(BIF::NET::IUdp& udp, const SacnConfig& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
        sacn_detail::clampUniverse(_cfg.universe);
        sacn_detail::defaultName(_cfg);
    }

    void setConfig(const SacnConfig& cfg) noexcept
    {
        const uint16_t prev = _cfg.universe;
        _cfg = cfg;
        sacn_detail::clampUniverse(_cfg.universe);
        sacn_detail::defaultName(_cfg);
        if (_isOpen && _cfg.multicast && prev != _cfg.universe)
            retargetGroup(prev);
    }

    [[nodiscard]] const SacnConfig& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(kSacnPort))
            return false;
        if (_cfg.multicast) {
            _udp.setTtl(1);
            if (!_udp.joinGroup(net::sacnGroup(_cfg.universe))) {
                _udp.close();
                return false;
            }
        }
        _isOpen = true;
        _frameReady = false;
        return true;
    }

    void close() override
    {
        if (_isOpen && _cfg.multicast)
            _udp.leaveGroup(net::sacnGroup(_cfg.universe));
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

        uint8_t pkt[kSacnMax];
        BIF::NET::Endpoint src{};
        for (;;) {
            const std::size_t n = _udp.recvFrom(pkt, sizeof(pkt), src);
            if (n == 0u)
                break;
            Frame parsed{};
            if (!sacn_detail::decode(pkt, n, parsed, _cfg.universe)) {
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
    void retargetGroup(uint16_t prev) noexcept
    {
        _udp.leaveGroup(net::sacnGroup(prev));
        _udp.joinGroup(net::sacnGroup(_cfg.universe));
    }

    BIF::NET::IUdp& _udp;
    SacnConfig _cfg;
    Frame _rx{};
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
