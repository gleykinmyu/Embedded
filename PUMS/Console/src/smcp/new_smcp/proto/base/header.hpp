/**
 * @file new_smcp/proto/base/header.hpp
 * @brief smcp::proto::base::Header — 17 бит, MSB-first.
 *
 *  16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |p1|p0|d6|d5|d4|d3|d2|d1|d0|s6|s5|s4|s3|s2|s1|s0|pr|
 * +-----+--------------------+--------------------+--+
 * |prio |        dst         |        src         |pr|
 * |2 bit|       7 bit        |       7 bit        |1b|
 * +-----+--------------------+--------------------+--+
 *
 * prio  — меньшее значение выигрывает арбитраж.
 * dst   — кому, 0x00 никто, 0x7F все.
 * src   — от кого.
 * proto — следующий протокол.
 *
 * Потолок разрядности задан числом, а не последним именем enum.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "custom/extensions.hpp"
#include "smcp/new_smcp/proto/can.hpp"

namespace smcp::proto::base {

struct addr_t {
    static constexpr size_t kBits = 7;
    static constexpr uint8_t kMax = static_cast<uint8_t>((1u << kBits) - 1u);

    uint8_t v = 0;

    static const addr_t kNone;
    static const addr_t kBroadcast;
    static const addr_t kConsoleMin;
    static const addr_t kConsoleMax;
    static const addr_t kServerMin;
    static const addr_t kServerMax;

    constexpr addr_t() = default;
    constexpr addr_t(uint8_t x) noexcept : v(x) {}

    [[nodiscard]] bool accepts(addr_t dst) const noexcept
    {
        return dst == *this || dst == kBroadcast;
    }

    [[nodiscard]] bool isBroadcast() const noexcept { return *this == kBroadcast; }
    [[nodiscard]] bool isUnicast() const noexcept
    {
        return *this != kNone && !isBroadcast();
    }
    [[nodiscard]] bool isConsole() const noexcept
    {
        return v >= kConsoleMin.v && v <= kConsoleMax.v;
    }
    [[nodiscard]] bool isServer() const noexcept
    {
        return v >= kServerMin.v && v <= kServerMax.v;
    }

    friend bool operator==(const addr_t& a, const addr_t& b) noexcept { return a.v == b.v; }
    friend bool operator!=(const addr_t& a, const addr_t& b) noexcept { return a.v != b.v; }

    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<kBits>(s, v);
    }
};

inline constexpr addr_t addr_t::kNone{0x00u};
inline constexpr addr_t addr_t::kBroadcast{addr_t::kMax};
inline constexpr addr_t addr_t::kConsoleMin{0x01u};
inline constexpr addr_t addr_t::kConsoleMax{0x0Fu};
inline constexpr addr_t addr_t::kServerMin{0x10u};
inline constexpr addr_t addr_t::kServerMax{static_cast<uint8_t>(addr_t::kMax - 1u)};

static_assert(addr_t::kMax == 0x7Fu);
static_assert(addr_t::kServerMax.v == 0x7Eu);

enum class Priority : uint8_t {
    Low = 3,
    Mid = 2,
    High = 1,
    Max = 0,
};

enum class Proto : uint8_t {
    Control = 0,
    Message = 1,
};

struct Header {
    static constexpr size_t kPrioBits = 2;
    static constexpr size_t kProtoBits = 1;
    static constexpr size_t kBits = kPrioBits + addr_t::kBits + addr_t::kBits + kProtoBits;

    Priority prio = Priority::Low;
    addr_t dst{};
    addr_t src{};
    Proto proto = Proto::Control;

    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<kPrioBits>(s, prio);
        s.object(dst);
        s.object(src);
        bitsery::custom::putBits<kProtoBits>(s, proto);
    }
};

static_assert(CanLimits::kFrameBits >= Header::kBits);
static_assert(Header::kBits == 17u);

inline constexpr size_t kPayloadBits = CanLimits::kFrameBits - Header::kBits;

static_assert(kPayloadBits == 76u);

} // namespace smcp::proto::base
