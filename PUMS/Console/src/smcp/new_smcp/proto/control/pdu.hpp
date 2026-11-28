/**
 * @file new_smcp/proto/control/pdu.hpp
 * @brief PDU протокола Control. Каждый тип сериализуется сам.
 *
 * После Header::op, без выравнивания:
 *
 * HeartBeat, msg = Request | Ack | InvalidVersion.
 * version: major, minor, patch. patch — uint16, младший байт первый.
 * +--------+--------+--------+------------------+
 * |  msg   | major  | minor  |      patch       |
 * | 8 bit  | 8 bit  | 8 bit  |     2 bytes      |
 * +--------+--------+--------+------------------+
 *
 * HeartBeat, msg = Busy. Тело кончается на msg.
 * +--------+
 * |  msg   |
 * | 8 bit  |
 * +--------+
 *
 * Fault. detail — uint16 LE. Имя — 7 байт.
 * +------------------+--------------------+
 * |      detail      |        name        |
 * |    2 bytes       |      7 bytes       |
 * +------------------+--------------------+
 *
 * Arp. oper — байт. Имя — 8 байт. Req спрашивает имя, Rep отвечает.
 * Адрес узла — src базового заголовка. Оба кадра всем.
 * +--------+------------------------------+
 * |  oper  |             name             |
 * | 8 bit  |           8 bytes            |
 * +--------+------------------------------+
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "fw_version.hpp"
#include "smcp/new_smcp/proto/control/header.hpp"

namespace smcp::proto::control {

struct HeartBeat {
    enum class Msg : uint8_t {
        Request = 0x00,
        Ack = 0x01,
        Busy = 0x02,
        InvalidVersion = 0x03,
    };

    static constexpr Type kId = Type::HeartBeat;
    static constexpr size_t kMsgBits = 8;
    static constexpr size_t kVersionBits = 32;
    static constexpr size_t kBusyBits = kMsgBits;
    static constexpr size_t kBits = kMsgBits + kVersionBits;

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

static_assert(HeartBeat::kBits <= kPayloadBits);
static_assert(HeartBeat::kBusyBits <= kPayloadBits);
static_assert(HeartBeat::kBits % 8u == 0u);

struct Fault {
    static constexpr Type kId = Type::Fault;
    static constexpr size_t kDetailBits = 16;
    static_assert(kPayloadBits >= kDetailBits);
    static_assert((kPayloadBits - kDetailBits) % 8u == 0u);
    static constexpr size_t kNameLen = (kPayloadBits - kDetailBits) / 8u;
    static constexpr size_t kBits = kDetailBits + kNameLen * 8u;

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

static_assert(Fault::kNameLen == 7u);
static_assert(Fault::kBits == kPayloadBits);

struct Arp {
    enum class Op : uint8_t {
        Req = 0x00,
        Rep = 0x01,
    };

    static constexpr Type kId = Type::Arp;
    static constexpr size_t kOpBits = 8;
    static_assert(kPayloadBits >= kOpBits);
    static_assert((kPayloadBits - kOpBits) % 8u == 0u);
    static constexpr size_t kNameLen = (kPayloadBits - kOpBits) / 8u;
    static constexpr size_t kBits = kOpBits + kNameLen * 8u;

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

static_assert(Arp::kNameLen == 8u);
static_assert(Arp::kBits == kPayloadBits);

} // namespace smcp::proto::control
