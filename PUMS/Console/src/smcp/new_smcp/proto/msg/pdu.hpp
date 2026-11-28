/**
 * @file new_smcp/proto/msg/pdu.hpp
 * @brief Служебные PDU Message. Идентификатор — 5 бит, отдельно от Header.
 *
 * CtrlHeader
 * +--+--+--+--+--+
 * |i4|i3|i2|i1|i0|
 * +--------------+
 * |      id      |
 * |    5 bit     |
 * +--------------+
 *
 * Ack — тело пустое.
 *
 * Nack — error, один байт.
 * +--------+
 * | error  |
 * | 8 bit  |
 * +--------+
 *
 * Сегмент. seg — uint16 LE.
 * 0x0000 Begin, 0x0001…0xFFFE Data, 0xFFFF End.
 * SegData — фиксированный массив, длина в поток не пишется.
 * SegEnd.len — байт, сколько байт последнего Data действительны.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "bitsery/custom/extensions.hpp"
#include "smcp/new_smcp/proto/msg/header.hpp"

namespace smcp::proto::msg {

enum class CtrlId : uint8_t {
    Ack = 0x00,
    Nack = 0x01,
    SegData = 0x02,
};

struct CtrlHeader {
    static constexpr size_t kIdBits = 5;
    static constexpr size_t kBits = kIdBits;

    CtrlId id = CtrlId::Ack;

    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<kIdBits>(s, id);
    }
};

static_assert(kPayloadBits >= CtrlHeader::kBits);
static_assert(CtrlHeader::kBits == 5u);

inline constexpr size_t kCtrlPayloadBits = kPayloadBits - CtrlHeader::kBits;

static_assert(kCtrlPayloadBits == 64u);

struct Ack {
    static constexpr CtrlId kId = CtrlId::Ack;
    static constexpr size_t kBits = 0;

    template <typename S>
    void serialize(S&)
    {
    }
};

static_assert(Ack::kBits <= kCtrlPayloadBits);

struct Nack {
    static constexpr CtrlId kId = CtrlId::Nack;
    static constexpr size_t kErrorBits = 8;
    static constexpr size_t kBits = kErrorBits;

    /** Транспортный отказ. Сегментные — срыв Begin / Data / End. */
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

static_assert(Nack::kBits <= kCtrlPayloadBits);
static_assert(Nack::kBits % 8u == 0u);

namespace seg {

struct SegHeader {
    static constexpr size_t kSegBits = 16;
    static constexpr size_t kBits = kSegBits;
    static constexpr uint16_t kBegin = 0x0000;
    static constexpr uint16_t kEnd = 0xFFFF;

    uint16_t seg = kBegin;

    [[nodiscard]] bool isBegin() const noexcept { return seg == kBegin; }
    [[nodiscard]] bool isEnd() const noexcept { return seg == kEnd; }

    template <typename S>
    void serialize(S& s)
    {
        s.value2b(seg);
    }
};

static_assert(kCtrlPayloadBits >= SegHeader::kBits);

inline constexpr size_t kSegPayloadBits = kCtrlPayloadBits - SegHeader::kBits;

static_assert(kSegPayloadBits % 8u == 0u);

inline constexpr size_t kSegDataMax = kSegPayloadBits / 8u;

static_assert(kSegDataMax == 6u);

struct SegBegin {
    static constexpr size_t kBits = 16;

    uint16_t count = 0;

    template <typename S>
    void serialize(S& s)
    {
        s.value2b(count);
    }
};

struct SegData {
    static constexpr size_t kDataMax = kSegDataMax;
    static constexpr size_t kBits = kDataMax * 8u;

    uint8_t data[kDataMax]{};

    template <typename S>
    void serialize(S& s)
    {
        s.container1b(data);
    }
};

struct SegEnd {
    static constexpr size_t kLenBits = 8;
    static constexpr size_t kCrcBits = 32;
    static constexpr size_t kBits = kLenBits + kCrcBits;
    static constexpr size_t kLenMax = (size_t{1} << kLenBits) - 1u;

    uint8_t len = 0;
    uint32_t crc = 0;

    template <typename S>
    void serialize(S& s)
    {
        s.value1b(len);
        s.value4b(crc);
    }
};

static_assert(SegBegin::kBits <= kSegPayloadBits);
static_assert(SegData::kBits == kSegPayloadBits);
static_assert(SegEnd::kBits <= kSegPayloadBits);
static_assert(kSegDataMax <= SegEnd::kLenMax);
static_assert(SegEnd::kLenBits == 8u);

inline constexpr size_t kSegCountMax = static_cast<size_t>(SegHeader::kEnd) - 1u;
inline constexpr size_t kMessageSizeMax = kSegCountMax * kSegDataMax;

static_assert(kSegCountMax == 65534u);
static_assert(kMessageSizeMax == 393204u);

/** Число пакетов Data. Begin и End в это число не входят. */
[[nodiscard]] constexpr size_t dataCount(size_t messageSize) noexcept
{
    if (messageSize == 0u) {
        return 0u;
    }
    return (messageSize + kSegDataMax - 1u) / kSegDataMax;
}

/** Действительные байты последнего Data. 0, если Data нет. */
[[nodiscard]] constexpr size_t lastLen(size_t messageSize) noexcept
{
    if (messageSize == 0u) {
        return 0u;
    }
    return (messageSize - 1u) % kSegDataMax + 1u;
}

/** Нули в конце последнего Data. Полный последний пакет даёт 0. */
[[nodiscard]] constexpr size_t padBytes(size_t messageSize) noexcept
{
    if (messageSize == 0u) {
        return 0u;
    }
    return kSegDataMax - lastLen(messageSize);
}

/** Длина сообщения по числу Data и длине последнего. count == 0 — пустое. */
[[nodiscard]] constexpr size_t messageSize(size_t count, size_t last) noexcept
{
    if (count == 0u) {
        return 0u;
    }
    return (count - 1u) * kSegDataMax + last;
}

static_assert(base::Header::kBits + Header::kBits + CtrlHeader::kBits + SegHeader::kBits + SegBegin::kBits == 61u);
static_assert(base::Header::kBits + Header::kBits + CtrlHeader::kBits + SegHeader::kBits + SegData::kBits == 93u);
static_assert(base::Header::kBits + Header::kBits + CtrlHeader::kBits + SegHeader::kBits + SegEnd::kBits == 85u);
static_assert((61u - CanLimits::kIdBits) / 8u == 4u);
static_assert((93u - CanLimits::kIdBits) / 8u == 8u);
static_assert((85u - CanLimits::kIdBits) / 8u == 7u);

} // namespace seg

} // namespace smcp::proto::msg
