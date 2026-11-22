/**
 * @file new_smcp/proto/msg/frame.hpp
 * @brief smcp::proto::msg::Id + Frame — один enum на весь узел (транспорт + northbound).
 *
 * Нумерация (меньший id — выше приоритет на шине):
 *   Ack/Nack/Fault  0x01…
 *   команды         0x10…0x2F
 *   HB              0x30
 *   config Get/Set  0x31…0x34
 *   Telemetry       0x40…
 */

#pragma once

#include <cstdint>

#include "comm/payload.hpp"
#include "smcp/new_smcp/proto/base/frame.hpp"

namespace smcp::proto::msg {

inline constexpr uint8_t kTxnIdMax = 0x3Fu;
inline constexpr uint8_t kIndexAll = 0xFFu;
inline constexpr uint8_t kInlineMax = 6u; ///< 8 − (tag|index) или 8 − (seq u16)

enum class Type : uint8_t {
    CtrlMsg = 0x00,
    AppMsg = 0x01,
};

using payload_t = comm::Payload<0, base::payload_t::kLenMax - base::payload_t::kLenMin>;

enum class CtrlId : uint8_t {
    Ack = 0x00,
    Nack = 0x01,
    SegData = 0x02,
};

/** CtrlId, msg_id = CtrlId::Ack. Тело пустое, txn в заголовке кадра. */
struct Ack {
    static constexpr CtrlId kId = CtrlId::Ack;
};

/** CtrlId, msg_id = CtrlId::Nack. error — байт тела. */
struct Nack {
    static constexpr CtrlId kId = CtrlId::Nack;

    /** Транспортный отказ. Сегментные — срыв DataBegin / Data / End. */
    enum class Error : uint8_t {
        None = 0,
        NotSupported,
        Busy,
        Timeout,
        NotArmed,
        Seq,
        Overflow,
        Crc,
        Unexpected,
        Abort,
    };

    Error error = Error::None;
};

/**
 * CtrlId, msg_id = CtrlId::SegData.
 * seg в байтах 2..3: 0 — Begin (count), 0xFFFF — End (crc), иначе Data (payload).
 * На приёме число сегментов между Begin и End сверять с count.
 */
struct SegData {
    static constexpr uint16_t kBegin = 0;
    static constexpr uint16_t kEnd = 0xFFFFu;
    static constexpr CtrlId kId = CtrlId::SegData;
    static constexpr uint8_t kPayloadMax = payload_t::kLenMax - sizeof(uint16_t);

    uint16_t seg = kBegin;
    union {
        uint16_t count = 0;
        uint32_t crc;
        comm::Payload<0, kPayloadMax> payload;
    };

    [[nodiscard]] bool isBegin() const noexcept { return seg == kBegin; }
    [[nodiscard]] bool isEnd() const noexcept { return seg == kEnd; }
};

/** AppMsg: msg_id — 5 бит заголовка, payload — остаток тела. */
struct AppMsg {
    uint8_t msg_id = 0;
    payload_t payload{};
};

/** CtrlMsg: id — 5 бит заголовка, тело — Ack, Nack или SegData. */
struct CtrlMsg {
    CtrlId id;
    union {
        Ack ack;
        Nack nack;
        SegData segData;
    };
};

/*
 * Заголовок — слово BitPayload<12>: type бит 11, txn [10:5], msg_id [4:0]. В payload — pushU16.
 * type и txn общие. message начинается в [4:0] и продолжаеытся телом (байт 2..).
 *
 *  11 10  9  8  7  6  5  4  3  2  1  0  byte 2   byte 3   byte 4   byte 5   byte 6   byte 7   byte 8   byte 9
 * +--+--+--+--+--+--+--+--+--+--+--+--+--------+--------+--------+--------+--------+--------+--------+--------+
 * |ty|n5|n4|n3|n2|n1|n0|m4|m3|m2|m1|m0|  msg   |  msg   |  msg   |  msg   |  msg   |  msg   |  msg   |  msg   |
 * +--+-----------------+--------------------------------------------------------------------------------------+
 * |ty|       txn       |                                       message                                        |
 * |1b|      6 bit      |                                  5 bit + байты 2..9                                  |
 * +--+-----------------+--------------------------------------------------------------------------------------+
 *
 * Ack — тело пустое.
 *                      +--------------+
 *                      |     Ack      |
 *                      |    5 bit     |
 *                      +--------------+
 *
 * Nack — error байт 2.
 *                      +--------------+--------+
 *                      |     Nack     | error  |
 *                      |    5 bit     | byte 2 |
 *                      +--------------+--------+
 *
 * SegData. seg = 0 — Begin, count в теле, байты 4..5. Сверять с сегментами до End.
 *                      +--------------+-----------------+-----------------+
 *                      |   SegData    |       seg       |      count      |
 *                      |    5 bit     |   bytes 2..3    |   bytes 4..5    |
 *                      +--------------+-----------------+-----------------+
 *
 * SegData. seg = 0xFFFF — End, crc32 в байтах 4..7.
 *                      +--------------+-----------------+-----------------------------------+
 *                      |   SegData    |       seg       |               crc32               |
 *                      |    5 bit     |   bytes 2..3    |            bytes 4..7             |
 *                      +--------------+-----------------+-----------------------------------+
 *
 * SegData. иначе Data — payload байты 4..9, до конца.
 *                      +--------------+-----------------+-----------------------------------------------------+
 *                      |   SegData    |       seg       |                       payload                       |
 *                      |    5 bit     |   bytes 2..3    |                     bytes 4..9                      |
 *                      +--------------+-----------------+-----------------------------------------------------+
 *
 * AppMsg — payload до конца байта 9.
 *                      +--------------+-----------------------------------------------------------------------+
 *                      |    msg_id    |                                payload                                |
 *                      |    5 bit     |                                остаток                                |
 *                      +--------------+-----------------------------------------------------------------------+
 */

struct Frame {
    Type type = Type::CtrlMsg;
    uint8_t txn;
    union {
        CtrlMsg ctrl;
        AppMsg app;
    };

    void pack(base::payload_t& out) const noexcept;
    [[nodiscard]] bool unpack(const base::payload_t& in) noexcept;
};

inline void Frame::pack(base::payload_t& out) const noexcept
{
    const uint8_t id = (type == Type::AppMsg) ? app.msg_id : static_cast<uint8_t>(ctrl.id);
    comm::BitPayload<12> head;
    head.push(type, 11, 1);
    head.push(txn, 5, 6);
    head.push(id, 0, 5);

    out.clear();
    out.push(head);

    if (type == Type::AppMsg) {
        out.push(app.payload);
        return;
    }

    switch (ctrl.id) {
    case CtrlId::Ack:
        break;
    case CtrlId::Nack:
        out.push(ctrl.nack.error);
        break;
    case CtrlId::SegData: {
        const SegData& seg = ctrl.segData;
        out.pushU16(seg.seg);
        if (seg.isBegin()) {
            out.pushU16(seg.count);
            break;
        }
        if (seg.isEnd()) {
            out.pushU32(seg.crc);
            break;
        }
        out.push(seg.payload);
        break;
    }
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
    uint8_t type_b = 0;
    uint8_t id = 0;
    if (!head.load(type_b, 11, 1) || 
        !head.load(txn, 5, 6) || 
        !head.load(id, 0, 5)) {
        return false;
    }
    type = static_cast<Type>(type_b);

    if (type == Type::AppMsg) {
        app.msg_id = id;
        return in.load(app.payload, base::payload_t::kLenMin);
    }

    ctrl.id = static_cast<CtrlId>(id);
    switch (ctrl.id) {
    case CtrlId::Ack:
        return in.len() == base::payload_t::kLenMin;
    case CtrlId::Nack:
        return in.len() == static_cast<uint8_t>(base::payload_t::kLenMin + 1u)
            && in.load(ctrl.nack.error, 2);
    case CtrlId::SegData: {
        if (in.len() < base::payload_t::kLenMin + sizeof(uint16_t)) {
            return false;
        }
        if (!in.load(ctrl.segData.seg, base::payload_t::kLenMin)) {
            return false;
        }
        const uint8_t off = static_cast<uint8_t>(base::payload_t::kLenMin + sizeof(uint16_t));
        const uint8_t n = static_cast<uint8_t>(in.len() - off);
        if (ctrl.segData.isBegin()) {
            return n == sizeof(uint16_t) && in.load(ctrl.segData.count, off);
        }
        if (ctrl.segData.isEnd()) {
            return n == sizeof(uint32_t) && in.load(ctrl.segData.crc, off);
        }
        if (n > SegData::kPayloadMax) {
            return false;
        }
        return in.load(ctrl.segData.payload, off);
    }
    }
    return false;
}

} // namespace smcp::proto::msg

/**
 * Demux PDU в onPacket(const Packet& pkt): id совпал и unpack удался — body в блоке.
 * Цепочка: else SMCP_IF_MSG (макрос сам if). Ждёт имя pkt в скоупе.
 *
 *   SMCP_IF_MSG(Fault) {
 *       ...
 *   } else SMCP_IF_MSG(Ack) {
 *       ...
 *   }
 *   return false;
 */
#define SMCP_IF_MSG(Type) \
    if (Type body{}; pkt.msg.id == Type::kId && body.unpack(pkt.msg))
