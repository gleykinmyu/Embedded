/**
 * @file new_smcp/proto/control/frame.hpp
 * @brief smcp::proto::control::Header — шапка протокола Control.
 *
 * Заголовок — op в data[0], 4 бита. Тело с data[1].
 * HeartBeat: msg в data[1]. Request, Ack, InvalidVersion — FwVersion в байтах 2..5. Busy — без версии.
 * Fault: detail (uint16) в байтах 1..2, name[7] в байтах 3..9.
 * Arp: oper в data[1], имя в байтах 2..9. Оба кадра всем, addr узла — src ответа.
 */

#pragma once

#include <cstdint>

#include "fw_version.hpp"
#include "smcp/new_smcp/proto/bitsery.hpp"

namespace MISC {

/** На проводе — pack(): patch_lo, patch_hi, minor, major. */
template <typename S>
void serialize(S& s, FwVersion& v)
{
    uint32_t raw = v.pack();
    s.value4b(raw);
    v = FwVersion::unpack(raw);
}

} // namespace MISC

namespace smcp::proto::control {

enum class Type : uint8_t {
    HeartBeat = 0x00,
    Fault = 0x01,
    Arp = 0x02,
};

inline constexpr uint8_t kBodyOff = 1;

using payload_t = comm::Payload<0, 10u - kBodyOff>;

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

    template <typename S>
    void serialize(S& s)
    {
        s.value1b(msg);
        if (msg == Msg::Busy) {
            return;
        }
        s.object(version);
    }
};

/** Type::Fault. detail — байты 1..2, name — остаток тела. */
struct Fault {
    static constexpr Type kId = Type::Fault;
    static constexpr uint8_t kNameLen = payload_t::kLenMax - sizeof(uint16_t);

    uint16_t detail = 0;
    char name[kNameLen]{};

    template <typename S>
    void serialize(S& s)
    {
        s.value2b(detail);
        auto* raw = reinterpret_cast<uint8_t*>(name);
        for (uint8_t i = 0; i < kNameLen; ++i) {
            s.value1b(raw[i]);
        }
    }
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

    template <typename S>
    void serialize(S& s)
    {
        s.value1b(op);
        auto* raw = reinterpret_cast<uint8_t*>(name);
        for (uint8_t i = 0; i < kNameLen; ++i) {
            s.value1b(raw[i]);
        }
    }
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

/** op — 4 бита, потолок 15. Тело кадра сюда не входит. */
struct Header {
    Type op = Type::HeartBeat;

    template <typename S>
    void serialize(S& s)
    {
        uint8_t op_b = 0;
        if constexpr (isWriter<S>) {
            op_b = static_cast<uint8_t>(static_cast<uint8_t>(op) & 0x0Fu);
        }
        s.ext(op_b, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{0x0F}});
        if constexpr (!isWriter<S>) {
            op = static_cast<Type>(op_b);
        }
    }
};

} // namespace smcp::proto::control
