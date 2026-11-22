/**
 * @file smcp/new2/proto/base.hpp
 * @brief Байтовый кадр: BaseHdr (4) + payload (10).
 *
 * Логический кадр:
 *
 *    0      1      2      3     4                    13
 *  +------+------+------+------+------------------------+
 *  | prio | dst  | src  | proto|        payload         |
 *  +------+------+------+------+------------------------+
 *  |  1   |  1   |  1   |  1   |           10           |
 *
 *   max: addr — kMaxAddr. SMCP2_CAN — prio 0x03, proto 0x01
 *        иначе — остальные поля 0xFF.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "byte_writer.hpp"
#include "comm/proto.hpp"
#include "iframestream.hpp"

namespace smcp2 {

#if defined(SMCP2_CAN)
inline constexpr uint8_t kPrioMax = 0x03u;
inline constexpr uint8_t kProtoMax = 0x01u;
inline constexpr unsigned kPrioBits = 2u;
inline constexpr unsigned kProtoBits = 1u;
#else
inline constexpr uint8_t kPrioMax = 0xFFu;
inline constexpr uint8_t kProtoMax = 0xFFu;
inline constexpr unsigned kPrioBits = 8u;
inline constexpr unsigned kProtoBits = 8u;
#endif

inline constexpr std::size_t kHdrLen = 4u;
inline constexpr std::size_t kPayload = 10u;

struct addr_t {
    uint8_t v = 0;

    static const addr_t kNone;
    static const addr_t kBroadcast;

    constexpr addr_t() = default;
    constexpr addr_t(uint8_t x) noexcept : v(x) {}

    [[nodiscard]] constexpr bool accepts(addr_t dst) const noexcept
    {
        return dst == *this || dst == kBroadcast;
    }

    [[nodiscard]] constexpr bool isBroadcast() const noexcept { return *this == kBroadcast; }

    friend constexpr bool operator==(addr_t a, addr_t b) noexcept { return a.v == b.v; }
    friend constexpr bool operator!=(addr_t a, addr_t b) noexcept { return a.v != b.v; }

    bool pack(mem::IByteWriter& wr) const {
        wr.push(v);
        return true;
    }

    bool unpack(const mem::IByteReader& br) {
        v = br.pull();
        return true;
    }
};

#if defined(SMCP2_CAN)
inline constexpr addr_t kMaxAddr{0x7Fu};
#else
inline constexpr addr_t kMaxAddr{0xFFu};
#endif

inline constexpr addr_t addr_t::kNone{};
inline constexpr addr_t addr_t::kBroadcast{kMaxAddr};

enum class BPType : uint8_t {
    Unknown = 0,
    UDP,
    TCP,
    test
};

struct BaseHdr {
    addr_t dst{};
    addr_t src{};
    uint8_t prio;
    BPType proto;
};

struct BaseFrame : public comm::Frame<BaseHdr> {
    
    bool pack(mem::IByteWriter& wr) const final {
        wr.push(hdr.prio);
        hdr.dst.pack(wr);
        hdr.src.pack(wr);
        wr.push(hdr.proto);
        return true;
    }

    bool unpack(const mem::IByteReader& br) final {
        hdr.prio = br.pull();
        hdr.dst.unpack(br);
        hdr.src.unpack(br);
        hdr.proto = static_cast<BPType>(br.pull());
        return true;
    }
};

struct BaseParam {
    addr_t dst{};
    uint8_t prio = 0;
    BPType proto = BPType::Unknown;
};

class BaseProto : public comm::IProtoBase<BaseParam> {
public:    
    BaseProto(mem::IByteWriter& lower) : _lower(lower) {
    }
    
    void setSelfAddr(addr_t addr) noexcept {
        _self = addr;
    }

    [[nodiscard]] bool send(const comm::IFrame& frame, const BaseParam& params) noexcept override
    {
        BaseFrame f;
        f.hdr.dst = params.dst;
        f.hdr.src = _self;
        f.hdr.prio = params.prio;
        f.hdr.proto = params.proto;
        f.payload = &frame;
        f.pack(_lower);
    }

    void onRecv(const mem::IByteReader& br) noexcept override
    {
        BaseFrame f;
        f.unpack(br);
        if (f.hdr.dst.accepts(_self)) {
        }
    }

private:
    addr_t _self;
    mem::IByteWriter& _lower;
};

} // namespace smcp2
