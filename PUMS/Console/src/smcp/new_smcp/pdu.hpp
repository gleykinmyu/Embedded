/**
 * @file new_smcp/pdu.hpp
 * @brief Тонкие PDU: pack/unpack в Message или Frame (без логики протоколов).
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>

#include "crc.hpp"
#include "mem/byte_writer.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/transport.hpp"

namespace smcp {

/** Макс. байт тела в одном Set: payload минус tag|index. */
inline constexpr uint8_t kInlineMax = Message::kMaxLen - 2u;

struct Ack {
    static constexpr MsgId kId = MsgId::Ack;
    static constexpr MISC::time_t kTimeoutMs = 100u;
    static constexpr uint8_t kRetry = 3u;

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
    }
    template <typename M>
    [[nodiscard]] bool unpack(const M& m) const noexcept
    {
        return m.id == kId && m.bodySize() == 0u;
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

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
        m.payload.push(error);
        m.payload.pushU32(detail);
    }
    template <typename M>
    [[nodiscard]] bool unpack(const M& m) noexcept
    {
        if (m.id != kId || m.bodySize() != 5u) {
            return false;
        }
        mem::IByteReader p{m.payload};
        detail = p.pullU32();
        error = static_cast<Err>(p.pull());
        return true;
    }
};

struct Fault {
    static constexpr MsgId kId = MsgId::Fault;
    static constexpr uint8_t kDetailMax = Message::kMaxLen - 1u;

    enum class Code : uint8_t {
        IdConflict = 0x01,
    };

    Code reason{};
    uint8_t detail_len = 0;
    uint8_t detail[kDetailMax]{};

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
        assert(detail_len <= kDetailMax);
        const uint8_t n = detail_len <= kDetailMax ? detail_len : kDetailMax;
        m.payload.push(reason);
        for (uint8_t i = 0; i < n; ++i) {
            m.payload.push(detail[i]);
        }
    }
    template <typename M>
    [[nodiscard]] bool unpack(const M& m) noexcept
    {
        if (m.id != kId || m.bodySize() < 1u) {
            return false;
        }
        mem::IByteReader p{m.payload};
        detail_len = static_cast<uint8_t>(p.cursor() - 1u);
        if (detail_len > kDetailMax) {
            detail_len = kDetailMax;
        }
        while (p.cursor() > static_cast<std::size_t>(detail_len) + 1u) {
            (void)p.pull();
        }
        for (uint8_t i = detail_len; i > 0u; --i) {
            detail[i - 1u] = p.pull();
        }
        reason = static_cast<Code>(p.pull());
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

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
        if (msg != Msg::KeepAlive) {
            m.payload.push(msg);
        }
    }
    template <typename M>
    [[nodiscard]] bool unpack(const M& m) noexcept
    {
        if (m.id != kId) {
            return false;
        }
        if (m.bodySize() == 0u) {
            msg = Msg::KeepAlive;
            return true;
        }
        if (m.bodySize() != 1u) {
            return false;
        }
        mem::IByteReader p{m.payload};
        msg = static_cast<Msg>(p.pull());
        return true;
    }
};

struct Get {
    static constexpr MsgId kId = MsgId::Get;
    uint8_t tag = 0;
    uint8_t index = kIndexAll;

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
        m.payload.push(tag);
        m.payload.push(index);
    }
    template <typename M>
    [[nodiscard]] bool unpack(const M& m) noexcept
    {
        if (m.id != kId || m.bodySize() != 2u) {
            return false;
        }
        mem::IByteReader p{m.payload};
        index = p.pull();
        tag = p.pull();
        return true;
    }
};

struct Set {
    static constexpr MsgId kId = MsgId::Set;
    uint8_t tag = 0;
    uint8_t index = kIndexAll;
    uint8_t len = 0;
    uint8_t data[kInlineMax]{};

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
        assert(len <= kInlineMax);
        const uint8_t n = len <= kInlineMax ? len : kInlineMax;
        m.payload.push(tag);
        m.payload.push(index);
        for (uint8_t i = 0; i < n; ++i) {
            m.payload.push(data[i]);
        }
    }
    template <typename M>
    [[nodiscard]] bool unpack(const M& m) noexcept
    {
        if (m.id != kId || m.bodySize() < 2u) {
            return false;
        }
        mem::IByteReader p{m.payload};
        len = static_cast<uint8_t>(p.cursor() - 2u);
        if (len > kInlineMax) {
            len = kInlineMax;
        }
        while (p.cursor() > static_cast<std::size_t>(len) + 2u) {
            (void)p.pull();
        }
        for (uint8_t i = len; i > 0u; --i) {
            data[i - 1u] = p.pull();
        }
        index = p.pull();
        tag = p.pull();
        return true;
    }
};

/**
 * Единственный кадр длинного тела.
 * [0..1] seq. Дальше kPayload байт.
 * seq = 0: uint16 число чанков данных и CRC32 тела. Другая длина — не кадр.
 * seq ≥ 1: байты чанка. Промежуточный чанк полный, последний может быть короче.
 */
struct SeqData {
    static constexpr MsgId kId = MsgId::SeqData;
    static constexpr MISC::time_t kTimeoutMs = 100u;
    static constexpr uint8_t kPayload = Message::kMaxLen - 2u;

    uint16_t seq = 0;
    union {
        struct {
            uint16_t chunks;
            MISC::Crc32 crc32;
        } first;
        struct {
            uint8_t len;
            uint8_t data[kPayload];
        } chunk;
    };

    SeqData() noexcept : first{} {}

    template <typename M>
    void pack(M& m) const noexcept
    {
        m.clear();
        m.id = kId;
        m.payload.pushU16(seq);
        if (seq == 0u) {
            m.payload.pushU16(first.chunks);
            m.payload.pushU32(first.crc32.value());
            return;
        }
        assert(chunk.len <= kPayload);
        const uint8_t n = chunk.len <= kPayload ? chunk.len : kPayload;
        for (uint8_t i = 0; i < n; ++i) {
            m.payload.push(chunk.data[i]);
        }
    }

    template <typename M>
    [[nodiscard]] bool unpack(const M& m) noexcept
    {
        if (m.id != kId || m.bodySize() < 2u) {
            return false;
        }
        mem::IByteReader p{m.payload};
        const uint8_t* d = p.data();
        seq = static_cast<uint16_t>(d[0] | (static_cast<uint16_t>(d[1]) << 8));
        if (seq == 0u) {
            if (m.bodySize() != 2u + 2u + 4u) {
                return false;
            }
            first.crc32.setValue(p.pullU32());
            first.chunks = p.pullU16();
            seq = p.pullU16();
            return true;
        }
        chunk.len = static_cast<uint8_t>(p.cursor() - 2u);
        if (chunk.len == 0u || chunk.len > kPayload) {
            return false;
        }
        for (uint8_t i = chunk.len; i > 0u; --i) {
            chunk.data[i - 1u] = p.pull();
        }
        seq = p.pullU16();
        return true;
    }
};

} // namespace smcp
