/**
 * @file artnet.hpp
 * @brief Art-Net 4 OpDmx поверх IUdp. INode — логика, Node<N> — реестр привязок.
 *
 * В ObjRegistry слот 0…N-1, не Art-Net universe id (он 0…32767).
 * Universe живёт снаружи; Binding хранит указатель, seq и physical.
 */
#pragma once

#include "../dmx_data.hpp"
#include "iudp.hpp"
#include "netutil.hpp"
#include "obj_registry.hpp"

#include <cstring>

namespace dmx::artnet {

inline constexpr uint16_t kPort = 6454u;
inline constexpr uint16_t kOpDmx = 0x5000u;
inline constexpr uint16_t kProtVer = 14u;
inline constexpr std::size_t kHdr = 18u;
inline constexpr std::size_t kMax = kHdr + kMaxChannels;
inline constexpr uint8_t kMaxSlots = 16u;

/// Слот реестра: привязка к Universe + поля ArtDmx.
class Binding {
public:
    Universe* universe = nullptr;
    uint8_t physical = 0;
    uint8_t seq = 1;

    [[nodiscard]] uint8_t id() const noexcept { return _id; }

private:
    template <typename, typename>
    friend class MISC::ObjRegistry;

    void set_id(uint8_t id) noexcept { _id = id; }

    uint8_t _id = 0;
};

class INode {
public:
    using BindReg = MISC::ObjRegistry<Binding, uint8_t>;

    INode(const INode&) = delete;
    INode& operator=(const INode&) = delete;
    virtual ~INode() = default;

    void setDest(BIF::NET::Ipv4 ip) noexcept { _destIp = ip; }
    [[nodiscard]] BIF::NET::Ipv4 dest() const noexcept { return _destIp; }

    bool open() noexcept
    {
        if (_open)
            return true;
        if (!_udp.open(kPort))
            return false;
        _open = true;
        return true;
    }

    void close() noexcept
    {
        if (!_open)
            return;
        _udp.close();
        _open = false;
        _ready = false;
    }

    [[nodiscard]] bool isOpen() const noexcept { return _open; }

    /// nullptr — снять все. Иначе свободный слот реестра, ключ — uni->id.
    bool bind(Universe* uni) noexcept
    {
        if (uni == nullptr) {
            clearBinds();
            return true;
        }
        if (findByPtr(uni) != nullptr || findByArtId(uni->id) != nullptr) {
            Binding* b = findByPtr(uni);
            if (b == nullptr)
                b = findByArtId(uni->id);
            b->universe = uni;
            return true;
        }
        Binding* slot = freeBind();
        if (slot == nullptr)
            return false;
        slot->universe = uni;
        slot->physical = 0;
        slot->seq = 1;
        return true;
    }

    bool unbind(Universe* uni) noexcept
    {
        Binding* b = findByPtr(uni);
        if (b == nullptr)
            return false;
        b->universe = nullptr;
        b->seq = 1;
        b->physical = 0;
        return true;
    }

    bool send() noexcept
    {
        if (!_open)
            return false;

        bool ok = false;
        forEach([&](Binding& b) {
            if (b.universe == nullptr)
                return true;
            uint8_t pkt[kMax]{};
            const std::size_t n = encode(pkt, *b.universe, b);
            const BIF::NET::Endpoint dest{_destIp, kPort};
            if (!_udp.sendTo(pkt, n, dest)) {
                ok = false;
                return false;
            }
            ok = true;
            ++_frameCount;
            return true;
        });
        return ok;
    }

    void poll() noexcept
    {
        if (!_open)
            return;

        uint8_t pkt[kMax];
        BIF::NET::Endpoint src{};
        const std::size_t n = _udp.recvFrom(pkt, sizeof(pkt), src);
        if (n == 0u)
            return;

        Universe parsed{};
        uint8_t seq = 0;
        uint8_t physical = 0;
        const Parse p = decode(pkt, n, parsed, seq, physical);
        if (p == Parse::Ignore)
            return;
        if (p != Parse::Ok) {
            _status = Status::DataError;
            return;
        }

        Binding* b = findByArtId(parsed.id);
        if (b == nullptr || b->universe == nullptr)
            return;

        b->seq = seq;
        b->physical = physical;
        *b->universe = parsed;
        _ready = true;
        ++_frameCount;
    }

    bool recv() noexcept
    {
        if (!_ready)
            return false;
        _ready = false;
        return true;
    }

    [[nodiscard]] std::size_t available() const noexcept { return _ready ? 1u : 0u; }
    void purge() noexcept { _ready = false; }
    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearErrors() noexcept { _status = Status::OK; }
    [[nodiscard]] uint32_t frameCount() const noexcept { return _frameCount; }

    [[nodiscard]] BindReg& binds() noexcept { return _binds; }
    [[nodiscard]] const BindReg& binds() const noexcept
    {
        return const_cast<INode*>(this)->_binds;
    }

protected:
    INode(BIF::NET::IUdp& udp, BindReg& binds) noexcept
        : _udp(udp)
        , _binds(binds)
    {
    }

private:
    enum class Parse : uint8_t { Ok, Ignore, Error };

    static constexpr char kId[8] = {'A', 'r', 't', '-', 'N', 'e', 't', '\0'};

    template <typename F>
    void forEach(F&& fn) noexcept
    {
        const uint8_t first = _binds.firstId();
        const uint8_t end = _binds.endId();
        for (uint8_t id = first; id < end; ++id) {
            Binding* b = _binds.get(id);
            if (b == nullptr)
                continue;
            if (!fn(*b))
                break;
        }
    }

    void clearBinds() noexcept
    {
        forEach([](Binding& b) {
            b.universe = nullptr;
            b.seq = 1;
            b.physical = 0;
            return true;
        });
    }

    [[nodiscard]] Binding* freeBind() noexcept
    {
        Binding* found = nullptr;
        forEach([&](Binding& b) {
            if (b.universe == nullptr) {
                found = &b;
                return false;
            }
            return true;
        });
        return found;
    }

    [[nodiscard]] Binding* findByPtr(Universe* uni) noexcept
    {
        if (uni == nullptr)
            return nullptr;
        Binding* found = nullptr;
        forEach([&](Binding& b) {
            if (b.universe == uni) {
                found = &b;
                return false;
            }
            return true;
        });
        return found;
    }

    [[nodiscard]] Binding* findByArtId(uint16_t artId) noexcept
    {
        artId = static_cast<uint16_t>(artId & 0x7FFFu);
        Binding* found = nullptr;
        forEach([&](Binding& b) {
            if (b.universe != nullptr && (b.universe->id & 0x7FFFu) == artId) {
                found = &b;
                return false;
            }
            return true;
        });
        return found;
    }

    static void bumpSeq(Binding& b) noexcept
    {
        if (b.seq == 255u)
            b.seq = 1;
        else
            ++b.seq;
    }

    static std::size_t encode(uint8_t* pkt, const Universe& uni, Binding& b) noexcept
    {
        std::size_t n = kMaxChannels;
        if ((n & 1u) != 0u)
            ++n;

        const uint16_t universe = static_cast<uint16_t>(uni.id & 0x7FFFu);
        std::memcpy(pkt, kId, 8u);
        net::putLe16(pkt + 8, kOpDmx);
        pkt[10] = 0;
        pkt[11] = static_cast<uint8_t>(kProtVer);
        pkt[12] = b.seq;
        bumpSeq(b);
        pkt[13] = b.physical;
        pkt[14] = static_cast<uint8_t>(universe);
        pkt[15] = static_cast<uint8_t>(universe >> 8);
        net::putBe16(pkt + 16, static_cast<uint16_t>(n));
        std::memcpy(pkt + kHdr, uni.data.channels, kMaxChannels);
        if (n > kMaxChannels)
            std::memset(pkt + kHdr + kMaxChannels, 0, n - kMaxChannels);
        return kHdr + n;
    }

    static Parse decode(const uint8_t* pkt, std::size_t n, Universe& out, uint8_t& seq,
                        uint8_t& physical) noexcept
    {
        if (n < 12u || std::memcmp(pkt, kId, 8u) != 0)
            return Parse::Ignore;
        if (net::le16(pkt + 8) != kOpDmx)
            return Parse::Ignore;
        if (n < kHdr + 2u || pkt[11] < 14u)
            return Parse::Error;

        out.id = static_cast<uint16_t>(pkt[14] | (static_cast<uint16_t>(pkt[15]) << 8));
        out.id = static_cast<uint16_t>(out.id & 0x7FFFu);
        seq = pkt[12];
        physical = pkt[13];

        uint16_t len = net::be16(pkt + 16);
        if (len < 2u || (len & 1u) != 0u)
            return Parse::Error;
        if (len > kMaxChannels)
            len = static_cast<uint16_t>(kMaxChannels);
        if (n < kHdr + len)
            return Parse::Error;

        std::memcpy(out.data.channels, pkt + kHdr, len);
        if (len < kMaxChannels)
            std::memset(out.data.channels + len, 0, kMaxChannels - len);
        return Parse::Ok;
    }

    BIF::NET::IUdp& _udp;
    BindReg& _binds;
    BIF::NET::Ipv4 _destIp = 0xFFFFFFFFu;
    Status _status = Status::OK;
    uint32_t _frameCount = 0;
    bool _open = false;
    bool _ready = false;
};

template <uint8_t N = kMaxSlots>
class Node : public INode {
    static_assert(N > 0u, "artnet::Node: N > 0");

public:
    static constexpr uint8_t kBindMax = N;

    explicit Node(BIF::NET::IUdp& udp) noexcept
        : INode(udp, _store)
    {
        for (uint8_t i = 0; i < N; ++i)
            (void)_store.registerAt(i, &_pool[i]);
    }

private:
    Binding _pool[N]{};
    MISC::ObjStorage<Binding, N, uint8_t, 0> _store{};
};

} // namespace dmx::artnet
