/**
 * @file new_smcp/pdu.hpp
 * @brief Тонкие PDU: pack/unpack через comm::Payload.
 *
 * pack пишет с начала payload и оставляет длину в payload.len().
 * unpack читает тот же префикс через load(offset), в порядке pack.
 */

#pragma once

#include <cassert>
#include <cstdint>

#include "comm/payload.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/proto/msg/frame.hpp"

namespace smcp {

struct Ack {
    static constexpr proto::msg::Id kId = proto::msg::Id::Ack;
    static constexpr MISC::time_t kTimeoutMs = 100u;
    static constexpr uint8_t kRetry = 3u;

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        m.payload.clear();
    }

    [[nodiscard]] bool unpack(const proto::msg::Frame& m) const noexcept
    {
        return m.id == kId && m.payload.len() == 0u;
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
    static constexpr proto::msg::Id kId = proto::msg::Id::Nack;
    static constexpr uint32_t kDetailNone = 0u;

    Err error = Err::None;
    uint32_t detail = kDetailNone;

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        m.payload.clear();
        m.payload.push(error);
        m.payload.pushU32(detail);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() != 5u) {
            return false;
        }
        return m.payload.load(error, 0) && m.payload.load(detail, 1);
    }
};

struct Fault {
    static constexpr proto::msg::Id kId = proto::msg::Id::Fault;
    static constexpr uint8_t kDetailMax = proto::msg::Frame::kMaxDlc - 1u;

    enum class Code : uint8_t {
        IdConflict = 0x01,
    };

    Code reason{};
    uint8_t detail_len = 0;
    uint8_t detail[kDetailMax]{};

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        assert(detail_len <= kDetailMax);
        const uint8_t n = detail_len <= kDetailMax ? detail_len : kDetailMax;
        m.payload.clear();
        m.payload.push(reason);
        m.payload.push(detail, n);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() < 1u || m.payload.len() > proto::msg::Frame::kMaxDlc) {
            return false;
        }
        detail_len = static_cast<uint8_t>(m.payload.len() - 1u);
        if (!m.payload.load(reason, 0)) {
            return false;
        }
        return detail_len == 0u || m.payload.load(detail, detail_len, 1);
    }
};

struct Heartbeat {
    static constexpr proto::msg::Id kId = proto::msg::Id::Heartbeat;
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

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        m.payload.clear();
        if (msg != Msg::KeepAlive) {
            m.payload.push(msg);
        }
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId) {
            return false;
        }
        if (m.payload.len() == 0u) {
            msg = Msg::KeepAlive;
            return true;
        }
        if (m.payload.len() != 1u) {
            return false;
        }
        return m.payload.load(msg, 0);
    }
};

struct Get {
    static constexpr proto::msg::Id kId = proto::msg::Id::Get;
    uint8_t tag = 0;
    uint8_t index = proto::msg::kIndexAll;

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        m.payload.clear();
        m.payload.push(tag);
        m.payload.push(index);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() != 2u) {
            return false;
        }
        return m.payload.load(tag, 0) && m.payload.load(index, 1);
    }
};

struct Set {
    static constexpr proto::msg::Id kId = proto::msg::Id::Set;
    uint8_t tag = 0;
    uint8_t index = proto::msg::kIndexAll;
    uint8_t len = 0;
    uint8_t data[proto::msg::kInlineMax]{};

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        assert(len <= proto::msg::kInlineMax);
        const uint8_t n = len <= proto::msg::kInlineMax ? len : proto::msg::kInlineMax;
        m.payload.clear();
        m.payload.push(tag);
        m.payload.push(index);
        m.payload.push(data, n);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() < 2u || m.payload.len() > proto::msg::Frame::kMaxDlc) {
            return false;
        }
        len = static_cast<uint8_t>(m.payload.len() - 2u);
        if (!m.payload.load(tag, 0) || !m.payload.load(index, 1)) {
            return false;
        }
        return len == 0u || m.payload.load(data, len, 2);
    }
};

struct DataBegin {
    static constexpr proto::msg::Id kId = proto::msg::Id::DataBegin;
    static constexpr MISC::time_t kTimeoutMs = 100u;
    uint8_t tag = 0;
    uint8_t index = proto::msg::kIndexAll;
    uint16_t size = 0;
    uint32_t crc32 = 0;

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        m.payload.clear();
        m.payload.push(tag);
        m.payload.push(index);
        m.payload.pushU16(size);
        m.payload.pushU32(crc32);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() != 8u) {
            return false;
        }
        return m.payload.load(tag, 0) && m.payload.load(index, 1)
            && m.payload.load(size, 2) && m.payload.load(crc32, 4)
            && size >= 1u;
    }
};

struct Data {
    static constexpr proto::msg::Id kId = proto::msg::Id::Data;
    uint16_t seq = 0;
    uint8_t len = 0;
    uint8_t data[proto::msg::kInlineMax]{};

    void pack(proto::msg::Frame& m) const noexcept
    {
        m.id = kId;
        assert(len <= proto::msg::kInlineMax);
        const uint8_t n = len <= proto::msg::kInlineMax ? len : proto::msg::kInlineMax;
        m.payload.clear();
        m.payload.pushU16(seq);
        m.payload.push(data, n);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() < 2u || m.payload.len() > proto::msg::Frame::kMaxDlc) {
            return false;
        }
        len = static_cast<uint8_t>(m.payload.len() - 2u);
        if (!m.payload.load(seq, 0)) {
            return false;
        }
        return len == 0u || m.payload.load(data, len, 2);
    }
};

} // namespace smcp
