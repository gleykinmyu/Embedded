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
using Universe = BIF::dmx::Universe;
using Status = BIF::dmx::Status;
using iTx = BIF::dmx::iTx;
using iRx = BIF::dmx::iRx;

namespace ArtNet {

inline constexpr uint16_t kPort = 6454u;
inline constexpr uint16_t kOpDmx = 0x5000u;
inline constexpr uint16_t kProtVer = 14u;
inline constexpr std::size_t kDmxHdr = 18u;
inline constexpr std::size_t kDmxMax = kDmxHdr + BIF::dmx::kMaxChannels;
inline constexpr std::size_t kMaxSlots = 16u;

struct Config {
    uint16_t universe = 0; ///< RX-фильтр
    BIF::NET::Ipv4 destIp = 0xFFFFFFFFu; ///< куда слать UDP
};

/// Состояние ArtDmx на одну вселенную: seq/physical и указатели bind.
struct Slot {
    const Universe* src = nullptr;
    Universe* dst = nullptr;
    uint16_t universe = 0;
    uint8_t physical = 0;
    uint8_t seq = 1;
    bool used = false;
};

} // namespace ArtNet

namespace artnet_detail {

inline constexpr char kId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};

inline void bumpSeq(ArtNet::Slot& slot) noexcept
{
    if (slot.seq == 255u)
        slot.seq = 1;
    else
        ++slot.seq;
}

inline void resetSlots(ArtNet::Slot* slots, std::size_t n) noexcept
{
    for (std::size_t i = 0; i < n; ++i)
        slots[i] = ArtNet::Slot{};
}

inline ArtNet::Slot* bindSlot(ArtNet::Slot* slots, std::size_t n, uint16_t universe) noexcept
{
    universe = static_cast<uint16_t>(universe & 0x7FFFu);
    for (std::size_t i = 0; i < n; ++i) {
        if (slots[i].used && slots[i].universe == universe)
            return &slots[i];
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (!slots[i].used) {
            slots[i].src = nullptr;
            slots[i].dst = nullptr;
            slots[i].used = true;
            slots[i].universe = universe;
            slots[i].physical = 0;
            slots[i].seq = 1;
            return &slots[i];
        }
    }
    return nullptr;
}

inline bool unbindSrc(ArtNet::Slot* slots, std::size_t n, const Universe* uni) noexcept
{
    if (uni == nullptr)
        return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (slots[i].used && slots[i].src == uni) {
            slots[i] = ArtNet::Slot{};
            return true;
        }
    }
    return false;
}

inline bool unbindDst(ArtNet::Slot* slots, std::size_t n, Universe* uni) noexcept
{
    if (uni == nullptr)
        return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (slots[i].used && slots[i].dst == uni) {
            slots[i] = ArtNet::Slot{};
            return true;
        }
    }
    return false;
}

inline std::size_t encode(uint8_t* pkt, const Universe& uni, ArtNet::Slot& slot) noexcept
{
    std::size_t n = BIF::dmx::kMaxChannels;
    if ((n & 1u) != 0u)
        ++n;

    const uint16_t universe = static_cast<uint16_t>(uni.id & 0x7FFFu);
    std::memcpy(pkt, kId, 8u);
    net::putLe16(pkt + 8, ArtNet::kOpDmx);
    pkt[10] = 0;
    pkt[11] = static_cast<uint8_t>(ArtNet::kProtVer);
    pkt[12] = slot.seq;
    bumpSeq(slot);
    pkt[13] = slot.physical;
    pkt[14] = static_cast<uint8_t>(universe);
    pkt[15] = static_cast<uint8_t>(universe >> 8);
    net::putBe16(pkt + 16, static_cast<uint16_t>(n));
    std::memcpy(pkt + ArtNet::kDmxHdr, uni.data.channels, BIF::dmx::kMaxChannels);
    if (n > BIF::dmx::kMaxChannels)
        std::memset(pkt + ArtNet::kDmxHdr + BIF::dmx::kMaxChannels, 0, n - BIF::dmx::kMaxChannels);
    return ArtNet::kDmxHdr + n;
}

enum class Parse : uint8_t { Ok, Ignore, Error };

inline Parse decode(const uint8_t* pkt, std::size_t n, Universe& out, uint8_t& seq,
                    uint8_t& physical) noexcept
{
    if (n < 12u || std::memcmp(pkt, kId, 8u) != 0)
        return Parse::Ignore;
    if (net::le16(pkt + 8) != ArtNet::kOpDmx)
        return Parse::Ignore;
    if (n < ArtNet::kDmxHdr + 2u || pkt[11] < 14u)
        return Parse::Error;

    out.id = static_cast<uint16_t>(pkt[14] | (static_cast<uint16_t>(pkt[15]) << 8));
    out.id = static_cast<uint16_t>(out.id & 0x7FFFu);
    seq = pkt[12];
    physical = pkt[13];

    uint16_t len = net::be16(pkt + 16);
    if (len < 2u || (len & 1u) != 0u)
        return Parse::Error;
    if (len > BIF::dmx::kMaxChannels)
        len = static_cast<uint16_t>(BIF::dmx::kMaxChannels);
    if (n < ArtNet::kDmxHdr + len)
        return Parse::Error;

    std::memcpy(out.data.channels, pkt + ArtNet::kDmxHdr, len);
    if (len < BIF::dmx::kMaxChannels)
        std::memset(out.data.channels + len, 0, BIF::dmx::kMaxChannels - len);
    return Parse::Ok;
}

} // namespace artnet_detail

class ArtNetTx : public iTx {
public:
    explicit ArtNetTx(BIF::NET::IUdp& udp, const ArtNet::Config& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
    }

    void setConfig(const ArtNet::Config& cfg) noexcept { _cfg = cfg; }
    [[nodiscard]] const ArtNet::Config& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(ArtNet::kPort))
            return false;
        artnet_detail::resetSlots(_slots, ArtNet::kMaxSlots);
        _isOpen = true;
        return true;
    }

    void close() override
    {
        _udp.close();
        _isOpen = false;
    }

    [[nodiscard]] bool isOpen() const override { return _isOpen; }

    bool bind(const Universe* uni) override
    {
        if (uni == nullptr) {
            artnet_detail::resetSlots(_slots, ArtNet::kMaxSlots);
            return true;
        }
        ArtNet::Slot* slot = artnet_detail::bindSlot(_slots, ArtNet::kMaxSlots, uni->id);
        if (slot == nullptr)
            return false;
        slot->src = uni;
        return true;
    }

    bool unbind(const Universe* uni) override
    {
        return artnet_detail::unbindSrc(_slots, ArtNet::kMaxSlots, uni);
    }

    bool send() override
    {
        if (!_isOpen)
            return false;

        bool ok = false;
        for (std::size_t i = 0; i < ArtNet::kMaxSlots; ++i) {
            ArtNet::Slot& slot = _slots[i];
            if (!slot.used || slot.src == nullptr)
                continue;

            uint8_t pkt[ArtNet::kDmxMax]{};
            const std::size_t n = artnet_detail::encode(pkt, *slot.src, slot);
            BIF::NET::Endpoint dest{_cfg.destIp, ArtNet::kPort};
            if (!_udp.sendTo(pkt, n, dest))
                return false;
            ok = true;
            ++_frameCount;
        }
        return ok;
    }

    Status getStatus() override { return _status; }
    void clearErrors() override { _status = Status::OK; }
    [[nodiscard]] uint32_t frameCount() const override { return _frameCount; }

private:
    BIF::NET::IUdp& _udp;
    ArtNet::Config _cfg;
    ArtNet::Slot _slots[ArtNet::kMaxSlots]{};
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    bool _isOpen = false;
};

class ArtNetRx : public iRx {
public:
    explicit ArtNetRx(BIF::NET::IUdp& udp, const ArtNet::Config& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
    }

    void setConfig(const ArtNet::Config& cfg) noexcept { _cfg = cfg; }
    [[nodiscard]] const ArtNet::Config& config() const noexcept { return _cfg; }

    bool open() override
    {
        if (!_udp.open(ArtNet::kPort))
            return false;
        artnet_detail::resetSlots(_slots, ArtNet::kMaxSlots);
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

    bool bind(Universe* uni) override
    {
        if (uni == nullptr) {
            artnet_detail::resetSlots(_slots, ArtNet::kMaxSlots);
            return true;
        }
        ArtNet::Slot* slot = artnet_detail::bindSlot(_slots, ArtNet::kMaxSlots, uni->id);
        if (slot == nullptr)
            return false;
        slot->dst = uni;
        return true;
    }

    bool unbind(Universe* uni) override
    {
        return artnet_detail::unbindDst(_slots, ArtNet::kMaxSlots, uni);
    }

    bool recv() override
    {
        if (!_frameReady)
            return false;
        _frameReady = false;
        return true;
    }

    void poll() override
    {
        if (!_isOpen)
            return;

        uint8_t pkt[ArtNet::kDmxMax];
        BIF::NET::Endpoint src{};
        const std::size_t n = _udp.recvFrom(pkt, sizeof(pkt), src);
        if (n == 0u)
            return;

        Universe parsed{};
        uint8_t seq = 0;
        uint8_t physical = 0;
        const auto parsedOk = artnet_detail::decode(pkt, n, parsed, seq, physical);
        if (parsedOk == artnet_detail::Parse::Ignore)
            return;
        if (parsedOk != artnet_detail::Parse::Ok) {
            _status = Status::DataError;
            return;
        }

        ArtNet::Slot* slot = nullptr;
        for (std::size_t i = 0; i < ArtNet::kMaxSlots; ++i) {
            if (_slots[i].used && _slots[i].dst != nullptr && _slots[i].universe == parsed.id) {
                slot = &_slots[i];
                break;
            }
        }
        if (slot == nullptr)
            return;

        slot->seq = seq;
        slot->physical = physical;
        *slot->dst = parsed;
        _frameReady = true;
        ++_frameCount;
    }

    [[nodiscard]] std::size_t available() const override { return _frameReady ? 1u : 0u; }
    void purge() override { _frameReady = false; }
    Status getStatus() override { return _status; }
    void clearErrors() override { _status = Status::OK; }
    [[nodiscard]] uint32_t frameCount() const override { return _frameCount; }

private:
    BIF::NET::IUdp& _udp;
    ArtNet::Config _cfg;
    ArtNet::Slot _slots[ArtNet::kMaxSlots]{};
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
