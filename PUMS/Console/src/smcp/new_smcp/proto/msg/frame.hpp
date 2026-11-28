/**
 * @file new_smcp/proto/msg/frame.hpp
 * @brief smcp::proto::msg::Header — шапка протокола Message.
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

#include "smcp/new_smcp/proto/bitsery.hpp"

namespace smcp::proto::msg {

/** Байт тела после слова заголовка BitPayload<12> (uint16). */
inline constexpr uint8_t kBodyOff = sizeof(uint16_t);
inline constexpr uint8_t kTxnIdMax = 0x3Fu;
inline constexpr uint8_t kIndexAll = 0xFFu;
inline constexpr uint8_t kInlineMax = 6u; ///< 8 − (tag|index) или 8 − (seq u16)

enum class Type : uint8_t {
    CtrlMsg = 0x00,
    AppMsg = 0x01,
};


enum class CtrlId : uint8_t {
    Ack = 0x00,
    Nack = 0x01,
    SegData = 0x02,
};

/** CtrlId, msg_id = CtrlId::Ack. Тело пустое, txn в заголовке кадра. */
struct Ack {
    static constexpr CtrlId kId = CtrlId::Ack;

    template <typename S>
    void serialize(S&) {}
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

    template <typename S>
    void serialize(S& s)
    {
        s.value1b(error);
    }
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

    /** seg = 0. count — байты 4..5. */
    struct Begin {
        uint16_t count = 0;

        template <typename S>
        void serialize(S& s)
        {
            s.value2b(count);
        }
    };

    /** seg = 0xFFFF. crc32 — байты 4..7. */
    struct End {
        uint32_t crc = 0;

        template <typename S>
        void serialize(S& s)
        {
            s.value4b(crc);
        }
    };

    /** Иначе Data. payload — байты 4.. до конца. */
    struct Data {
        comm::Payload<0, kPayloadMax> payload{};

        template <typename S>
        void serialize(S& s)
        {
            s.ext(payload, WireTail{});
        }
    };

    uint16_t seg = kBegin;
    union {
        Begin begin{};
        End end;
        Data data;
    };

    [[nodiscard]] bool isBegin() const noexcept { return seg == kBegin; }
    [[nodiscard]] bool isEnd() const noexcept { return seg == kEnd; }

    template <typename S>
    void serialize(S& s)
    {
        s.value2b(seg);
        if (isBegin()) {
            s.object(begin);
            return;
        }
        if (isEnd()) {
            s.object(end);
            return;
        }
        s.object(data);
    }
};

/** AppMsg: payload — байты после слова кадра. msg_id пишет Header. */
struct AppMsg {
    uint8_t msg_id = 0;
    payload_t payload{};

    template <typename S>
    void serialize(S& s)
    {
        s.ext(payload, WireTail{});
    }
};

/** CtrlMsg: тело Ack, Nack или SegData. id пишет Header. */
struct CtrlMsg {
    CtrlId id = CtrlId::Ack;
    union {
        Ack ack{};
        Nack nack;
        SegData segData;
    };

    template <typename S>
    void serialize(S& s)
    {
        switch (id) {
        case CtrlId::Ack:
            s.object(ack);
            break;
        case CtrlId::Nack:
            s.object(nack);
            break;
        case CtrlId::SegData:
            s.object(segData);
            break;
        }
    }
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

/** 12 бит: type, txn, id. Тело кадра сюда не входит. */
struct Header {
    Type type = Type::CtrlMsg;
    uint8_t txn = 0;
    uint8_t id = 0;

    template <typename S>
    void serialize(S& s)
    {
        s.ext(type, bitsery::ext::ValueRange<Type>{Type::CtrlMsg, Type::AppMsg});
        uint8_t txn_b = 0;
        uint8_t id_b = 0;
        if constexpr (isWriter<S>) {
            txn_b = static_cast<uint8_t>(txn & kTxnIdMax);
            id_b = static_cast<uint8_t>(id & 0x1Fu);
        }
        s.ext(txn_b, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{kTxnIdMax}});
        s.ext(id_b, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{0x1Fu}});
        if constexpr (!isWriter<S>) {
            txn = txn_b;
            id = id_b;
        }
    }
};

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
