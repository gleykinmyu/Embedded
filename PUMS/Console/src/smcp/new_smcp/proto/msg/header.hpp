/**
 * @file new_smcp/proto/msg/header.hpp
 * @brief smcp::proto::msg::Header — txn, затем type. 7 бит.
 *
 * Порядок намеренно другой, чем у прежнего слова (type, txn, id).
 * Меньший txn теперь ближе к старшим битам CAN ID и раньше выигрывает арбитраж.
 * Пять бит идентификатора PDU сюда не входят: их пишет следующий заголовок.
 *
 * +--+--+--+--+--+--+--+
 * |n5|n4|n3|n2|n1|n0|ty|
 * +--------------------+--+
 * |        txn         |ty|
 * |       6 bit        |1b|
 * +--------------------+--+
 *
 * type = 0 — служебный PDU Message. type = 1 — сообщение верхнего уровня.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "custom/extensions.hpp"
#include "smcp/new_smcp/proto/base/header.hpp"

namespace smcp::proto::msg {

enum class Type : uint8_t {
    CtrlMsg = 0x00,
    AppMsg = 0x01,
};

struct Header {
    static constexpr size_t kTxnBits = 6;
    static constexpr size_t kTypeBits = 1;
    static constexpr size_t kBits = kTxnBits + kTypeBits;
    static constexpr uint8_t kTxnMax = static_cast<uint8_t>((1u << kTxnBits) - 1u);

    uint8_t txn = 0;
    Type type = Type::CtrlMsg;

    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<kTxnBits>(s, txn);
        bitsery::custom::putBits<kTypeBits>(s, type);
    }
};

static_assert(Header::kTxnMax == 0x3Fu);
static_assert(base::kPayloadBits >= Header::kBits);
static_assert(Header::kBits == 7u);

inline constexpr size_t kPayloadBits = base::kPayloadBits - Header::kBits;

static_assert(kPayloadBits == 69u);

} // namespace smcp::proto::msg
