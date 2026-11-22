/**
 * @file new_smcp/proto/control/frame.hpp
 * @brief smcp::proto::control::Frame — кадр протокола Control.
 *
 * Заголовок — op в младших 8 битах слова BitPayload<12>.
 * Ping и Pong: MISC::FwVersion::pack() в байтах 2..5 (uint32, младший байт первый).
 * Fault: detail (uint16) в байтах 2..3, name[6] в байтах 4..9.
 */

#pragma once

#include <cstdint>

#include "comm/payload.hpp"
#include "fw_version.hpp"
#include "smcp/new_smcp/proto/base/frame.hpp"

namespace smcp::proto::control {

enum class Type : uint8_t {
    Ping = 0x00,
    Pong = 0x01,
    Fault = 0x02,
};

using payload_t = comm::Payload<0, base::payload_t::kLenMax - base::payload_t::kLenMin>;

/** Type::Ping. version — прошивка, на проводе FwVersion::pack(), байты 2..5. */
struct Ping {
    static constexpr Type kId = Type::Ping;
    MISC::FwVersion version{};
};

/** Type::Pong. version — прошивка, на проводе FwVersion::pack(), байты 2..5. */
struct Pong {
    static constexpr Type kId = Type::Pong;
    MISC::FwVersion version{};
};

/** Type::Fault. detail — байты 2..3, name — остаток тела. */
struct Fault {
    static constexpr Type kId = Type::Fault;
    static constexpr uint8_t kNameLen = payload_t::kLenMax - sizeof(uint16_t);

    uint16_t detail = 0;
    char name[kNameLen]{};
};

/*
 * Заголовок — слово BitPayload<12>: op в битах [7:0], [11:8] — 0.
 *
 *  11 10  9  8  7  6  5  4  3  2  1  0  byte 2   byte 3   byte 4   byte 5
 * +--+--+--+--+--+--+--+--+--+--+--+--+--------+--------+--------+--------+
 * |     0     |          op           |                                   |
 * |   4 bit   |         8 bit         |                                   |
 * +-----------+-----------------------+-----------------------------------+
 *
 * Ping / Pong — FwVersion::pack() в байтах 2..5, младший байт первый.
 *                      +--------------+-----------------------------------+
 *                      |      op      |              version              |
 *                      |    8 bit     |             bytes 2..5            |
 *                      +--------------+-----------------------------------+
 *
 * Fault — detail байты 2..3, name байты 4..9.
 *                      +--------------+-----------------+-----------------------------------+
 *                      |    Fault     |     detail      |               name                |
 *                      |    8 bit     |   bytes 2..3    |            bytes 4..9             |
 *                      +--------------+-----------------+-----------------------------------+
 */

struct Frame {
    Type op = Type::Ping;
    union {
        Ping ping{};
        Pong pong;
        Fault fault;
    };

    void pack(base::payload_t& out) const noexcept;
    [[nodiscard]] bool unpack(const base::payload_t& in) noexcept;
};

inline void Frame::pack(base::payload_t& out) const noexcept
{
    comm::BitPayload<12> head;
    head.push(op, 0, 8);

    out.clear();
    out.push(head);

    switch (op) {
    case Type::Ping:
        out.pushU32(ping.version.pack());
        break;
    case Type::Pong:
        out.pushU32(pong.version.pack());
        break;
    case Type::Fault:
        out.pushU16(fault.detail);
        out.push(reinterpret_cast<const uint8_t*>(fault.name), Fault::kNameLen);
        break;
    }
}

inline bool Frame::unpack(const base::payload_t& in) noexcept
{
    if (in.len() < base::payload_t::kLenMin) {
        return false;
    }
    comm::BitPayload<12> head;
    if (!in.load(head, 0)) {
        return false;
    }
    uint8_t op_b = 0;
    if (!head.load(op_b, 0, 8)) {
        return false;
    }
    op = static_cast<Type>(op_b);

    switch (op) {
    case Type::Ping: {
        uint32_t raw = 0;
        const bool ok = in.len() == static_cast<uint8_t>(base::payload_t::kLenMin + sizeof(uint32_t))
            && in.load(raw, base::payload_t::kLenMin);
        if (ok) {
            ping.version = MISC::FwVersion::unpack(raw);
        }
        return ok;
    }
    case Type::Pong: {
        uint32_t raw = 0;
        const bool ok = in.len() == static_cast<uint8_t>(base::payload_t::kLenMin + sizeof(uint32_t))
            && in.load(raw, base::payload_t::kLenMin);
        if (ok) {
            pong.version = MISC::FwVersion::unpack(raw);
        }
        return ok;
    }
    case Type::Fault:
        return in.len() == static_cast<uint8_t>(base::payload_t::kLenMin + sizeof(uint16_t) + Fault::kNameLen)
            && in.load(fault.detail, base::payload_t::kLenMin)
            && in.load(reinterpret_cast<uint8_t*>(fault.name), Fault::kNameLen,
                static_cast<uint8_t>(base::payload_t::kLenMin + sizeof(uint16_t)));
    }
    return false;
}

} // namespace smcp::proto::control
