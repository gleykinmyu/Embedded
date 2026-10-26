/**
 * @file message.hpp
 * @brief Транспорт SMCP: конверт CAN (ID + payload), служебные PDU.
 *
 * Правила: PROTOCOL.md
 *
 * CAN ID (29 bit):
 *   msg_id[28:22] | dst[21:14] | src[13:6] | txn_id[5:0]
 * data[0..7] — непрозрачный payload (txn_id в ID, не в data).
 * Кадры вне транзакции: txn_id = 0.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ican.hpp"

namespace smcp {

namespace protocol {

inline constexpr uint8_t kMsgIdBits = 7u;
inline constexpr uint8_t kMsgIdMax = (1u << kMsgIdBits) - 1u;
inline constexpr uint8_t kTxnIdBits = 6u;
/** Макс. txn opener (1…kTxnIdMax); 0 — вне txn. */
inline constexpr uint8_t kTxnIdMax = (1u << kTxnIdBits) - 1u;

inline constexpr unsigned kCanIdTxnPos = 0;
inline constexpr unsigned kCanIdSrcPos = 6;
inline constexpr unsigned kCanIdDstPos = 14;
inline constexpr unsigned kCanIdMsgPos = 22;

} // namespace protocol

/**
 *   CAN ID (29 bit, IDE=1):
 *   +---------+--------+--------+---------+
 *   | msg_id  |  dst   |  src   | txn_id  |
 *   |  7 bit  | 8 bit  | 8 bit  |  6 bit  |
 *   +---------+--------+--------+---------+
 *   [28:22]    [21:14]  [13:6]   [5:0]
 *
 * Message — тип + payload. Packet — кадр: адреса, txn_id, Message.
 */
struct Message {
    static constexpr uint8_t kMaxDlc = 8u;

    uint8_t id = 0;
    uint8_t dlc = 0;
    uint8_t data[8]{};

    void setPayload(const uint8_t* src, uint8_t n) noexcept;
    [[nodiscard]] bool push(uint8_t b) noexcept;
    [[nodiscard]] bool pushU16(uint16_t v) noexcept;
    [[nodiscard]] bool pushU32(uint32_t v) noexcept;
    [[nodiscard]] uint16_t loadU16(uint8_t off) const noexcept;
    [[nodiscard]] uint32_t loadU32(uint8_t off) const noexcept;
    [[nodiscard]] bool isTransport() const noexcept;
};

struct Packet {
    static constexpr uint8_t kBroadcastId = 0xFFu;

    uint8_t src_id = 0;
    uint8_t dst_id = 0;
    uint8_t txn_id = 0;
    Message msg{};

    /** Кадр адресован этому узлу (или broadcast). */
    [[nodiscard]] bool isAddressedTo(uint8_t node_id) const noexcept;
    [[nodiscard]] bool pack(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] bool unpack(const BIF::CAN::Frame& frame) noexcept;
};

namespace msg {

/**
 * Служебные / config Id.
 * Link: 0x01…0x03, HB 0x30. Config: Get/Set + DataBegin/Data (0x31…0x34).
 * Прикладные northbound — в своих заголовках (0x10… / 0x40…).
 *
 * Inline vs multi: один порог @c kInlineMax (7 B) —
 * Set: payload в кадре Set; короткий ответ Get: один Data (seq|≤7).
 * Длиннее → DataBegin → Data* (size≥1; итог Set — Ack/Nack приёмника).
 * Get: ответ = Nack или тело; клиент после тела на wire не Ack'ает.
 */
inline constexpr uint8_t kInlineMax = 7u;

enum class Id : uint8_t {
    Ack         = 0x01,
    Nack        = 0x02,
    Fault       = 0x03, /**< Broadcast: узел снимает себя (сейчас IdConflict). */
    Heartbeat   = 0x30, /**< Ниже команд: keep-alive не забивает Select/SetTarget. */
    Get         = 0x31, /**< Открывает txn: чтение по tag. */
    Set         = 0x32, /**< Открывает txn: запись tag + ≤kInlineMax / длинное тело. */
    DataBegin   = 0x33, /**< Старт multi-тела: tag|size|crc32. */
    Data        = 0x34, /**< Кусок / короткий ответ Get: seq + ≤kInlineMax. */
};


[[nodiscard]] inline const char* cstr(Id id) noexcept
{
    switch (id) {
    case Id::Ack: return "Ack";
    case Id::Nack: return "Nack";
    case Id::Fault: return "Fault";
    case Id::Heartbeat: return "Heartbeat";
    case Id::Get: return "Get";
    case Id::Set: return "Set";
    case Id::DataBegin: return "DataBegin";
    case Id::Data: return "Data";
    }
    return "?";
}

[[nodiscard]] inline const char* cstr(uint8_t id) noexcept
{
    return cstr(static_cast<Id>(id));
}

#if defined(SMCP_TRACE_SHORT)
[[nodiscard]] inline const char* cstrS(Id id) noexcept
{
    switch (id) {
    case Id::Ack: return "Ack";
    case Id::Nack: return "Nk";
    case Id::Fault: return "Ft";
    case Id::Heartbeat: return "HB";
    case Id::Get: return "Get";
    case Id::Set: return "Set";
    case Id::DataBegin: return "DBg";
    case Id::Data: return "Dat";
    }
    return "?";
}

[[nodiscard]] inline const char* cstrS(uint8_t id) noexcept
{
    return cstrS(static_cast<Id>(id));
}
#endif

/**
 * Ack — DLC=0 (txn_id запроса в CAN ID).
 */
struct Ack {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Ack);
    static constexpr uint16_t kTimeoutMs = 100u;
    static constexpr uint8_t kRetry = 3u;

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Nack — DLC=5
 *   +-------+------------------+
 *   | error | detail (uint32)  |
 *   +-------+------------------+
 *    data[0]  data[1..4] LE     (txn_id запроса → CAN ID)
 *
 * @a error — код отказа (вышестоящий протокол). @a detail — непрозрачный;
 * northbound: Selection.raw() (биты осей). @c kDetailNone = пустая маска.
 * @c kTimeout — локальный abort Ack (не с шины).
 */
struct Nack {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Nack);
    static constexpr uint8_t kTimeout = 0x08u;
    static constexpr uint32_t kDetailNone = 0u;

    /**
     * Транспортный @c error (1…Max). Выше Max — прикладной код.
     * @c Body: причина в @c detail (= @c MultiByte::Error).
     */
    enum class Error : uint8_t {
        None = 0,
        NotSupported = 1, /**< Get/Set / onSetBuffer отказ. */
        Busy = 2,         /**< Уже идёт MultiByte xfer. */
        Body = 3,         /**< Сбой тела; detail = MultiByte::Error. */
        Max = 0x0Fu,      /**< Верхняя граница транспортных кодов. */
    };

    uint8_t error = 0;
    uint32_t detail = kDetailNone;

    [[nodiscard]] static Nack make(uint8_t error,
                                   uint32_t detail = kDetailNone) noexcept
    {
        Nack n{};
        n.error = error;
        n.detail = detail;
        return n;
    }

    [[nodiscard]] static Nack make(Error error,
                                   uint32_t detail = kDetailNone) noexcept
    {
        return make(static_cast<uint8_t>(error), detail);
    }

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;

    [[nodiscard]] static constexpr bool isTransport(uint8_t err) noexcept
    {
        return err >= static_cast<uint8_t>(Error::NotSupported)
            && err <= static_cast<uint8_t>(Error::Max);
    }

    [[nodiscard]] static const char* cstr(Error err) noexcept
    {
        switch (err) {
        case Error::None: return "Ok";
        case Error::NotSupported: return "NotSupported";
        case Error::Busy: return "Busy";
        case Error::Body: return "Body";
        case Error::Max: return "Max";
        }
        return "?";
    }
};

/**
 * Fault — DLC=1…8: error | detail[0…7]. Class D, txn_id=0, dst=FF.
 * Не ответ на запрос: узел объявляет свой отказ.
 * @c kIdConflict — дубль src на шине; detail = короткое имя узла (ASCII, ≤7).
 */
struct Fault {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Fault);
    static constexpr uint8_t kIdConflict = 0x01u;
    /** Макс. байт detail (Message::kMaxDlc − 1). */
    static constexpr uint8_t kDetailMax = Message::kMaxDlc - 1u;

    uint8_t error = 0;
    uint8_t detail_len = 0;
    uint8_t detail[kDetailMax]{};

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;

    /** Скопировать ASCII-имя в detail (обрезать до kDetailMax, без NUL на wire). */
    void setDetailStr(const char* s) noexcept;

    [[nodiscard]] static const char* cstr(uint8_t error) noexcept
    {
        return (error == kIdConflict) ? "IdConflict" : "?";
    }
};

/**
 * Heartbeat — DLC=0 (только ID, txn_id=0).
 */
struct Heartbeat {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Heartbeat);
    static constexpr uint16_t kTimeoutMs = 200u;
    /** Сколько интервалов T без RX HB до down. */
    static constexpr uint8_t kMissMax = 3u;

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Get — DLC=1. TaggedOps (не ReplyWnd): Nack **или** тело.
 *   +-----+
 *   | tag |
 *   +-----+
 * Клиент после тела Ack на wire не шлёт.
 */
struct Get {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Get);

    uint8_t tag = 0;

    [[nodiscard]] static Get make(uint8_t tag) noexcept
    {
        Get g{};
        g.tag = tag;
        return g;
    }

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Set — DLC=1…8 (TaggedOps, не ReplyWnd):
 *   DLC=1, len=0 → multi; DLC=2…8 → inline. Итог — Ack|Nack приёмника.
 */
struct Set {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Set);
    uint8_t tag = 0;
    uint8_t len = 0;
    uint8_t data[kInlineMax]{};

    /** Длинный opener: DLC=1, тело пойдёт DataBegin→Data*. */
    [[nodiscard]] static Set make(uint8_t tag) noexcept
    {
        Set s{};
        s.tag = tag;
        return s;
    }

    /** Короткий: payload в кадре Set (len 1…kInlineMax). */
    [[nodiscard]] static Set make(uint8_t tag, const uint8_t* src, uint8_t n) noexcept
    {
        Set s = make(tag);
        if (src == nullptr || n == 0u) {
            return s;
        }
        s.len = (n > kInlineMax) ? kInlineMax : n;
        std::memcpy(s.data, src, s.len);
        return s;
    }

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * DataBegin — старт multi-тела; корреляция — Packet.txn_id.
 * DLC=7:
 *   +-----+----------+------------+
 *   | tag | size LE  | crc32 LE   |
 *   +-----+----------+------------+
 * @c size ≥ 1 (пустой blob запрещён). Конец — по size + CRC у RX.
 * Итог Set-txn — Ack/Nack приёмника; Get-тело — без финального Ack клиента.
 */
struct DataBegin {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::DataBegin);
    /** Межфреймовый gap MultiByte (RX/TX progress); локальный abort. */
    static constexpr uint16_t kTimeoutMs = 100u;

    uint8_t tag = 0;
    uint16_t size = 0;
    uint32_t crc32 = 0;

    [[nodiscard]] static DataBegin make(uint8_t tag, uint16_t size,
                                        uint32_t crc32) noexcept
    {
        DataBegin b{};
        b.tag = tag;
        b.size = size;
        b.crc32 = crc32;
        return b;
    }

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Data — DLC=1…8: кусок тела или короткий ответ Get (≤kInlineMax payload).
 *   +-----+---------------------+
 *   | seq | data ≤kInlineMax    |
 *   +-----+---------------------+
 * После DataBegin: seq с 0. Короткий Get: один кадр seq=0, без Begin.
 * Корреляция — Packet.txn_id.
 */
struct Data {
    static constexpr uint8_t kId = static_cast<uint8_t>(Id::Data);
    static constexpr uint8_t kMaxPayload = kInlineMax;

    uint8_t seq = 0;
    uint8_t len = 0;
    uint8_t data[kMaxPayload]{};

    /** Короткий ответ Get: один кадр, seq=0. */
    [[nodiscard]] static Data make(const uint8_t* src, uint8_t n) noexcept
    {
        return make(0, src, n);
    }

    /** Кусок multi-тела (seq с 0 после DataBegin). */
    [[nodiscard]] static Data make(uint8_t seq, const uint8_t* src, uint8_t n) noexcept
    {
        Data d{};
        d.seq = seq;
        if (src == nullptr || n == 0u) {
            return d;
        }
        d.len = (n > kMaxPayload) ? kMaxPayload : n;
        std::memcpy(d.data, src, d.len);
        return d;
    }

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

[[nodiscard]] constexpr bool isBroadcastId(uint8_t id) noexcept
{
    return id == Packet::kBroadcastId;
}

} // namespace msg

} // namespace smcp

/**
 * Demux PDU в `onPacket(const Packet& pkt)`: id совпал и unpack удался — `body` в блоке.
 * Цепочка: `else SMCP_IF_MSG` (макрос сам `if`). Ждёт имя `pkt` в скоупе.
 *
 *   SMCP_IF_MSG(msg::Select) {
 *       onSelect(body, pkt.txn_id);
 *   } else SMCP_IF_MSG(msg::SetTarget) {
 *       onSetTarget(body, pkt.txn_id);
 *   } else {
 *       return false;
 *   }
 *   return true;
 */
#define SMCP_IF_MSG(Type) \
    if (Type body{}; pkt.msg.id == Type::kId && body.unpack(pkt.msg))
