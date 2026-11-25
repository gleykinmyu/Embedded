/**
 * @file new_smcp/proto/control/frame.hpp
 * @brief smcp::proto::control::Frame — кадр протокола Control.
 *
 * Заголовок — op в data[0], 4 бита. Тело с data[1].
 * HeartBeat: msg в data[1]. Request, Ack, InvalidVersion — FwVersion в байтах 2..5. Busy — без версии.
 * Fault: detail (uint16) в байтах 1..2, name[7] в байтах 3..9.
 * Arp: oper в data[1], имя в байтах 2..9. Оба кадра всем, addr узла — src ответа.
 */

#pragma once

#include <cstdint>

#include "comm/payload.hpp"
#include "fw_version.hpp"
#include "smcp/new_smcp/proto/base/frame.hpp"

namespace smcp::proto::control {

enum class Type : uint8_t {
    HeartBeat = 0x00,
    Fault = 0x01,
    Arp = 0x02,
};

inline constexpr uint8_t kBodyOff = 1;

using payload_t = comm::Payload<0, base::payload_t::kLenMax - kBodyOff>;

/**
 * Type::HeartBeat. msg в data[1].
 * Request — запрос, Ack — подтверждение, InvalidVersion — версия не сошлась.
 * Эти трое несут версию прошивки. Busy — узел занят, версии нет.
 */
struct HeartBeat {
    enum class Msg : uint8_t {
        Request = 0x00,
        Ack = 0x01,
        Busy = 0x02,
        InvalidVersion = 0x03,
    };

    static constexpr Type kId = Type::HeartBeat;

    Msg msg = Msg::Request;
    MISC::FwVersion version{};
};

/** Type::Fault. detail — байты 1..2, name — остаток тела. */
struct Fault {
    static constexpr Type kId = Type::Fault;
    static constexpr uint8_t kNameLen = payload_t::kLenMax - sizeof(uint16_t);

    uint16_t detail = 0;
    char name[kNameLen]{};
};

/**
 * Type::Arp. oper в data[1], имя следом.
 * Req — кто держит name. Rep — это моё имя, addr узла это src. Оба кадра всем.
 */
struct Arp {
    enum class Op : uint8_t {
        Req = 0x00,
        Rep = 0x01,
    };

    static constexpr Type kId = Type::Arp;
    static constexpr uint8_t kNameLen = payload_t::kLenMax - 1u;

    Op op = Op::Req;
    char name[kNameLen]{};
};

/*
 * Заголовок — op в data[0], биты [11:8]. Тело с data[1].
 *
 *  11 10  9  8  7  6  5  4  3  2  1  0  byte 2   byte 3   byte 4   byte 5   byte 6   byte 7   byte 8   byte 9
 * +--+--+--+--+--+--+--+--+--+--+--+--+--------+--------+--------+--------+--------+--------+--------+--------+
 * |    op     |        data[1]        |                                                                       |
 * |   4 bit   |         8 bit         |                                                                       |
 * +-----------+-----------------------+-----------------------------------------------------------------------+
 *
 * HeartBeat. msg = Request | Ack | InvalidVersion — version байты 2..5, младший байт первый.
 * +-----------+-----------------------+-----------------------------------+
 * |    op     |          msg          |              version              |
 * |   4 bit   |         8 bit         |            bytes 2..5             |
 * +-----------+-----------------------+-----------------------------------+
 *
 * HeartBeat. msg = Busy — тело кончается на msg.
 * +-----------+-----------------------+
 * |    op     |          msg          |
 * |   4 bit   |         8 bit         |
 * +-----------+-----------------------+
 *
 * Fault — detail байты 1..2, name байты 3..9.
 * +-----------+--------------------------------+--------------------------------------------------------------+
 * |    op     |             detail             |                            name                              |
 * |   4 bit   |           bytes 1..2           |                          bytes 3..9                          |
 * +-----------+--------------------------------+--------------------------------------------------------------+
 *
 * Arp. oper = Req | Rep, оба кадра всем. addr узла — src ответа. Имя байты 2..9.
 * +-----------+-----------------------+-----------------------------------------------------------------------+
 * |    op     |          oper         |                                 name                                  |
 * |   4 bit   |         8 bit         |                              bytes 2..9                               |
 * +-----------+-----------------------+-----------------------------------------------------------------------+
 */

struct Frame {
    Type op = Type::HeartBeat;
    union {
        HeartBeat hb{};
        Fault fault;
        Arp arp;
    };

    /** Дописывает в @a out. Буфер перед вызовом очищает вызывающий. */
    void pack(comm::PayloadWriter out) const noexcept;
    [[nodiscard]] bool unpack(comm::PayloadReader in) noexcept;
};

inline void Frame::pack(comm::PayloadWriter out) const noexcept
{
    comm::BitPayload<4> head;
    head.push(op, 0, 4);
    out.push(head);

    switch (op) {
    case Type::HeartBeat:
        out.push(hb.msg);
        if (hb.msg != HeartBeat::Msg::Busy) {
            out.pushU32(hb.version.pack());
        }
        break;
    case Type::Fault:
        out.pushU16(fault.detail);
        out.push(reinterpret_cast<const uint8_t*>(fault.name), Fault::kNameLen);
        break;
    case Type::Arp:
        out.push(arp.op);
        out.push(reinterpret_cast<const uint8_t*>(arp.name), Arp::kNameLen);
        break;
    }
}

inline bool Frame::unpack(comm::PayloadReader in) noexcept
{
    if (in.len() < kBodyOff) {
        return false;
    }
    comm::BitPayload<4> head;
    if (!in.load(head, 0)) {
        return false;
    }
    uint8_t op_b = 0;
    if (!head.load(op_b, 0, 4)) {
        return false;
    }
    op = static_cast<Type>(op_b);

    switch (op) {
    case Type::HeartBeat: {
        if (!in.load(hb.msg, kBodyOff)) {
            return false;
        }
        if (hb.msg == HeartBeat::Msg::Busy) {
            return in.len() == static_cast<uint8_t>(kBodyOff + 1u);
        }
        if (hb.msg != HeartBeat::Msg::Request && hb.msg != HeartBeat::Msg::Ack
            && hb.msg != HeartBeat::Msg::InvalidVersion) {
            return false;
        }
        uint32_t raw = 0;
        const uint8_t ver = static_cast<uint8_t>(kBodyOff + 1u);
        const bool ok = in.len() == static_cast<uint8_t>(ver + sizeof(uint32_t)) && in.load(raw, ver);
        if (ok) {
            hb.version = MISC::FwVersion::unpack(raw);
        }
        return ok;
    }
    case Type::Fault:
        return in.len() == static_cast<uint8_t>(kBodyOff + sizeof(uint16_t) + Fault::kNameLen)
            && in.load(fault.detail, kBodyOff)
            && in.load(reinterpret_cast<uint8_t*>(fault.name), Fault::kNameLen,
                static_cast<uint8_t>(kBodyOff + sizeof(uint16_t)));
    case Type::Arp: {
        const uint8_t name_at = static_cast<uint8_t>(kBodyOff + 1u);
        if (in.len() != static_cast<uint8_t>(name_at + Arp::kNameLen) || !in.load(arp.op, kBodyOff)) {
            return false;
        }
        if (arp.op != Arp::Op::Req && arp.op != Arp::Op::Rep) {
            return false;
        }
        return in.load(reinterpret_cast<uint8_t*>(arp.name), Arp::kNameLen, name_at);
    }
    }
    return false;
}

} // namespace smcp::proto::control
