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

inline constexpr uint16_t kSacnPort = 5568u;
inline constexpr std::size_t kSacnScOff = 125u;
inline constexpr std::size_t kSacnMax = kSacnScOff + 1u + kMaxChannels; // 638

class SacnPort : public IDmx {
public:
    struct Config {
        uint16_t universe = 1; ///< 1…63999
        uint8_t priority = 100;
        uint8_t cid[16]{};
        char sourceName[64]{};
        bool multicast = true;
        BIF::NET::Ipv4 destIp = 0; ///< unicast, если !multicast
    };

    explicit SacnPort(BIF::NET::IUdp& udp, const Config& cfg = {}) noexcept
        : _udp(udp)
        , _cfg(cfg)
    {
        if (_cfg.universe == 0u)
            _cfg.universe = 1u;
        if (_cfg.sourceName[0] == '\0') {
            const char* name = "DMX-Tester";
            std::size_t i = 0;
            while (name[i] != '\0' && i < 63u) {
                _cfg.sourceName[i] = name[i];
                ++i;
            }
        }
    }

    void setConfig(const Config& cfg) noexcept
    {
        const uint16_t prev = _cfg.universe;
        _cfg = cfg;
        if (_cfg.universe == 0u)
            _cfg.universe = 1u;
        if (_isOpen && _cfg.multicast && prev != _cfg.universe)
            retargetGroup(prev);
    }

    [[nodiscard]] const Config& config() const noexcept { return _cfg; }

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
        _seq = 0;
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
    [[nodiscard]] Transport transport() const override { return Transport::Sacn; }
    [[nodiscard]] Direction direction() const override { return _dir; }

    bool setDirection(Direction dir) override
    {
        _dir = dir;
        _frameReady = false;
        return true;
    }

    [[nodiscard]] uint16_t universe() const override { return _cfg.universe; }

    void setUniverse(uint16_t universe) override
    {
        if (universe == 0u)
            universe = 1u;
        if (universe > 63999u)
            universe = 63999u;
        const uint16_t prev = _cfg.universe;
        _cfg.universe = universe;
        if (_isOpen && _cfg.multicast && prev != universe)
            retargetGroup(prev);
    }

    bool send(const Frame& frame) override
    {
        if (!_isOpen || _dir != Direction::Transmit)
            return false;

        uint8_t pkt[kSacnMax]{};
        const std::size_t n = encode(pkt, frame);
        BIF::NET::Endpoint dest;
        dest.port = kSacnPort;
        dest.ip = _cfg.multicast ? net::sacnGroup(_cfg.universe) : _cfg.destIp;
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

        uint8_t pkt[kSacnMax];
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
    static constexpr char kAcnId[12] = {'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', '\0', '\0', '\0'};

    void retargetGroup(uint16_t prev) noexcept
    {
        _udp.leaveGroup(net::sacnGroup(prev));
        _udp.joinGroup(net::sacnGroup(_cfg.universe));
    }

    std::size_t encode(uint8_t* pkt, const Frame& frame) noexcept
    {
        uint16_t n = frame.count;
        if (n > kMaxChannels)
            n = static_cast<uint16_t>(kMaxChannels);
        if (n < 1u)
            n = 1u;

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
        std::memcpy(pkt + 22, _cfg.cid, 16u);

        net::putBe16(pkt + 38, static_cast<uint16_t>(0x7000u | framingPdu));
        net::putBe32(pkt + 40, 0x00000002u);
        std::memcpy(pkt + 44, _cfg.sourceName, 64u);
        pkt[108] = _cfg.priority;
        pkt[109] = 0;
        pkt[110] = 0;
        pkt[111] = _seq++;
        pkt[112] = 0;
        net::putBe16(pkt + 113, _cfg.universe);

        net::putBe16(pkt + 115, static_cast<uint16_t>(0x7000u | dmpPdu));
        pkt[117] = 0x02;
        pkt[118] = 0xA1;
        pkt[119] = 0x00;
        pkt[120] = 0x00;
        pkt[121] = 0x00;
        pkt[122] = 0x01;
        net::putBe16(pkt + 123, propCount);
        pkt[125] = frame.startCode;
        std::memcpy(pkt + 126, frame.slots, n);
        return static_cast<std::size_t>(126u + n);
    }

    bool decode(const uint8_t* pkt, std::size_t n, Frame& out) noexcept
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
        if (uni != _cfg.universe)
            return false;

        const uint16_t propCount = net::be16(pkt + 123);
        if (propCount < 2u)
            return false;
        uint16_t slots = static_cast<uint16_t>(propCount - 1u);
        if (slots > kMaxChannels)
            slots = static_cast<uint16_t>(kMaxChannels);
        if (n < 126u + slots)
            return false;

        out.startCode = pkt[125];
        out.count = slots;
        std::memcpy(out.slots, pkt + 126, slots);
        if (slots < kMaxChannels)
            std::memset(out.slots + slots, 0, kMaxChannels - slots);
        return true;
    }

    BIF::NET::IUdp& _udp;
    Config _cfg;
    Frame _rx{};
    Direction _dir = Direction::Receive;
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    uint8_t _seq = 0;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
