/**
 * @file new_smcp/pdu.hpp
 * @brief Тонкие PDU: pack/unpack в Message (без логики протоколов).
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>

#include "ms_timer.hpp"
#include "smcp/new_smcp/message.hpp"

namespace smcp {

struct Ack {
    static constexpr MsgId kId = MsgId::Ack;
    static constexpr MISC::time_t kTimeoutMs = 100u;
    static constexpr uint8_t kRetry = 3u;

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
    }
    [[nodiscard]] bool unpack(const Message& m) const noexcept
    {
        return m.id == kId && m.dlc == 0u;
    }
};

/** Транспортные коды Nack [First, Last). */
enum class Err : uint8_t {
    First = 0,
    None = First,
    NotSupported,
    Busy,
    Body,
    Timeout,
    Last = 0x10u,
};

struct Nack {
    static constexpr MsgId kId = MsgId::Nack;
    static constexpr uint32_t kDetailNone = 0u;

    Err error = Err::None;
    uint32_t detail = kDetailNone;

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        m.push(error);
        m.pushU32(detail);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId || m.dlc != 5u) {
            return false;
        }
        error = static_cast<Err>(m.data[0]);
        detail = m.loadU32(1);
        return true;
    }
};

struct Fault {
    static constexpr MsgId kId = MsgId::Fault;
    static constexpr uint8_t kDetailMax = Message::kMaxDlc - 1u;

    enum class Code : uint8_t {
        IdConflict = 0x01,
    };

    Code reason{};
    uint8_t detail_len = 0;
    uint8_t detail[kDetailMax]{};

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        assert(detail_len <= kDetailMax);
        const uint8_t n = detail_len <= kDetailMax ? detail_len : kDetailMax;
        m.push(reason);
        for (uint8_t i = 0; i < n; ++i) {
            m.push(detail[i]);
        }
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId || m.dlc < 1u) {
            return false;
        }
        reason = static_cast<Code>(m.data[0]);
        detail_len = static_cast<uint8_t>(m.dlc - 1u);
        if (detail_len > kDetailMax) {
            detail_len = kDetailMax;
        }
        if (detail_len != 0u) {
            std::memcpy(detail, m.data + 1, detail_len);
        }
        return true;
    }
};

struct Heartbeat {
    static constexpr MsgId kId = MsgId::Heartbeat;
    static constexpr MISC::time_t kTimeoutMs = 200u;
    static constexpr uint8_t kMissMax = 3u;

    /** Байт на проводе только пока линк устанавливается. KeepAlive — кадр пустой. */
    enum class Msg : uint8_t {
        KeepAlive = 0, ///< байта нет
        Request,       ///< мастер просит связь
        Ack,           ///< ведомый принимает
        Busy,          ///< свободной сессии нет
    };

    Msg msg = Msg::KeepAlive;

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        if (msg != Msg::KeepAlive) {
            m.push(msg);
        }
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId) {
            return false;
        }
        if (m.dlc == 0u) {
            msg = Msg::KeepAlive;
            return true;
        }
        if (m.dlc != 1u) {
            return false;
        }
        msg = static_cast<Msg>(m.data[0]);
        return true;
    }
};

struct Get {
    static constexpr MsgId kId = MsgId::Get;
    uint8_t tag = 0;
    uint8_t index = kIndexAll;

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        m.push(tag);
        m.push(index);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId || m.dlc != 2u) {
            return false;
        }
        tag = m.data[0];
        index = m.data[1];
        return true;
    }
};

struct Set {
    static constexpr MsgId kId = MsgId::Set;
    uint8_t tag = 0;
    uint8_t index = kIndexAll;
    uint8_t len = 0;
    uint8_t data[kInlineMax]{};

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        assert(len <= kInlineMax);
        const uint8_t n = len <= kInlineMax ? len : kInlineMax;
        m.push(tag);
        m.push(index);
        for (uint8_t i = 0; i < n; ++i) {
            m.push(data[i]);
        }
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId || m.dlc < 2u) {
            return false;
        }
        tag = m.data[0];
        index = m.data[1];
        len = static_cast<uint8_t>(m.dlc - 2u);
        if (len > kInlineMax) {
            len = kInlineMax;
        }
        if (len != 0u) {
            std::memcpy(data, m.data + 2, len);
        }
        return true;
    }
};

struct DataBegin {
    static constexpr MsgId kId = MsgId::DataBegin;
    static constexpr MISC::time_t kTimeoutMs = 100u;
    uint8_t tag = 0;
    uint8_t index = kIndexAll;
    uint16_t size = 0;
    uint32_t crc32 = 0;

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        m.push(tag);
        m.push(index);
        m.pushU16(size);
        m.pushU32(crc32);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId || m.dlc != 8u) {
            return false;
        }
        tag = m.data[0];
        index = m.data[1];
        size = m.loadU16(2);
        crc32 = m.loadU32(4);
        return size >= 1u;
    }
};

struct Data {
    static constexpr MsgId kId = MsgId::Data;
    uint16_t seq = 0;
    uint8_t len = 0;
    uint8_t data[kInlineMax]{};

    void pack(Message& m) const noexcept
    {
        m.clear();
        m.id = kId;
        assert(len <= kInlineMax);
        const uint8_t n = len <= kInlineMax ? len : kInlineMax;
        m.pushU16(seq);
        for (uint8_t i = 0; i < n; ++i) {
            m.push(data[i]);
        }
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.id != kId || m.dlc < 2u) {
            return false;
        }
        seq = m.loadU16(0);
        len = static_cast<uint8_t>(m.dlc - 2u);
        if (len > kInlineMax) {
            len = kInlineMax;
        }
        if (len != 0u) {
            std::memcpy(data, m.data + 2, len);
        }
        return true;
    }
};

} // namespace smcp
