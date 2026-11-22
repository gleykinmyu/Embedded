/**
 * @file new_smcp/proto/base/frame.hpp
 * @brief smcp::proto::base::Frame — кадр proto-слоя.
 *
 * CAN ID (29 bit): prio[28:27] | dst[26:20] | src[19:13] | proto[12]
 *                | data[0] биты 3:0 [11:8] | data[1] [7:0]
 * data кадра — data[2..]. Логический payload один: dlc = 2…10.
 */

#pragma once

#include <cstdint>

#include "comm/payload.hpp"

/*
 * Extended CAN ID, 29 бит (IDE=1). Бит 28 — старший: меньший prio выигрывает арбитраж.
 * Шапка — 17 бит. 12 бит ID — начало payload: старший байт (только младшие 4 бита) и data[1].
 *
 *  28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |p1|p0|d6|d5|d4|d3|d2|d1|d0|s6|s5|s4|s3|s2|s1|s0|pr|h3|h2|h1|h0|b7|b6|b5|b4|b3|b2|b1|b0|
 * +-----+--------------------+--------------------+--+-----------+-----------------------+
 * |prio |        dst         |        src         |pr|  data[0]  |        data[1]        |
 * |2 bit|       7 bit        |       7 bit        |1b|   4 bit   |         8 bit         |
 * +-----+--------------------+--------------------+--+-----------+-----------------------+
 *
 * data[0] — старший байт payload, работают только биты 3:0 (h3..h0). Биты 7:4 всегда 0.
 * data[2..9] лежат в data кадра CAN. dlc считает байты payload: минимум 2 (они всегда в ID),
 * дальше 0…8 байт кадра, поэтому dlc = 2…10.
 *
 * +----------+----------+----------+----------+----------+----------+----------+----------+
 * |  data 2  |  data 3  |  data 4  |  data 5  |  data 6  |  data 7  |  data 8  |  data 9  |
 * |  8 bit   |  8 bit   |  8 bit   |  8 bit   |  8 bit   |  8 bit   |  8 bit   |  8 bit   |
 * +----------+----------+----------+----------+----------+----------+----------+----------+
 *
 * prio     [28:27]  2 бит. Меньший — выше приоритет на шине.
 * dst      [26:20]  7 бит, кому.
 * src      [19:13]  7 бит, от кого.
 * proto    [12]     1 бит.
 * data[0]  [11:8]   младшие 4 бита старшего байта payload.
 * data[1]  [7:0]    второй байт payload.
 * data[2+]          байты кадра; сколько их — dlc − 2.
 */


namespace smcp::proto::base {

struct addr_t {
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
};

inline const addr_t addr_t::kNone{0x00u};
inline const addr_t addr_t::kBroadcast{0x7Fu};
inline const addr_t addr_t::kConsoleMin{0x01u};
inline const addr_t addr_t::kConsoleMax{0x0Fu};
inline const addr_t addr_t::kServerMin{0x10u};
inline const addr_t addr_t::kServerMax{0x7Eu};


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

using payload_t = comm::Payload<2, 10>;

struct Frame {

    Priority prio = Priority::Low;
    addr_t dst{};
    addr_t src{}; 
    Proto proto = Proto::Control;
    payload_t payload{};
};

} // namespace smcp::proto::base
