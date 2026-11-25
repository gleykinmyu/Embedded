/**
 * @file new_smcp/pdu.hpp
 * @brief Тонкие PDU: pack/unpack через comm::Payload.
 *
 * pack пишет через PayloadWriter и оставляет длину в payload.len().
 * unpack читает через PayloadReader::load(offset), в порядке pack.
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
        comm::PayloadWriter w{m.payload};
        w.push(error);
        w.pushU32(detail);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() != 5u) {
            return false;
        }
        comm::PayloadReader r{m.payload};
        return r.load(error, 0) && r.load(detail, 1);
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
        comm::PayloadWriter w{m.payload};
        w.push(reason);
        w.push(detail, n);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() < 1u || m.payload.len() > proto::msg::Frame::kMaxDlc) {
            return false;
        }
        detail_len = static_cast<uint8_t>(m.payload.len() - 1u);
        comm::PayloadReader r{m.payload};
        if (!r.load(reason, 0)) {
            return false;
        }
        return detail_len == 0u || r.load(detail, detail_len, 1);
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
            comm::PayloadWriter{m.payload}.push(msg);
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
        return comm::PayloadReader{m.payload}.load(msg, 0);
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
        comm::PayloadWriter w{m.payload};
        w.push(tag);
        w.push(index);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() != 2u) {
            return false;
        }
        comm::PayloadReader r{m.payload};
        return r.load(tag, 0) && r.load(index, 1);
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
        comm::PayloadWriter w{m.payload};
        w.push(tag);
        w.push(index);
        w.push(data, n);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() < 2u || m.payload.len() > proto::msg::Frame::kMaxDlc) {
            return false;
        }
        len = static_cast<uint8_t>(m.payload.len() - 2u);
        comm::PayloadReader r{m.payload};
        if (!r.load(tag, 0) || !r.load(index, 1)) {
            return false;
        }
        return len == 0u || r.load(data, len, 2);
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
        comm::PayloadWriter w{m.payload};
        w.push(tag);
        w.push(index);
        w.pushU16(size);
        w.pushU32(crc32);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() != 8u) {
            return false;
        }
        comm::PayloadReader r{m.payload};
        return r.load(tag, 0) && r.load(index, 1)
            && r.load(size, 2) && r.load(crc32, 4)
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
        comm::PayloadWriter w{m.payload};
        w.pushU16(seq);
        w.push(data, n);
    }
    [[nodiscard]] bool unpack(const proto::msg::Frame& m) noexcept
    {
        if (m.id != kId || m.payload.len() < 2u || m.payload.len() > proto::msg::Frame::kMaxDlc) {
            return false;
        }
        len = static_cast<uint8_t>(m.payload.len() - 2u);
        comm::PayloadReader r{m.payload};
        if (!r.load(seq, 0)) {
            return false;
        }
        return len == 0u || r.load(data, len, 2);
    }
};

} // namespace smcp
