/**
 * @file new_smcp/proto/control/header.hpp
 * @brief smcp::proto::control::Header — op, 4 бита.
 *
 * Потолок 0x0F. Новое имя enum ниже этого потолка ширину не меняет.
 * Тело PDU начинается со следующего бита, без выравнивания.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "bitsery/custom/extensions.hpp"
#include "smcp/new_smcp/proto/base/header.hpp"

namespace smcp::proto::control {

enum class Type : uint8_t {
    HeartBeat = 0x00,
    Fault = 0x01,
    Arp = 0x02,
};

struct Header {
    static constexpr size_t kOpBits = 4;
    static constexpr size_t kBits = kOpBits;

    Type op = Type::HeartBeat;

    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<kOpBits>(s, op);
    }
};

static_assert(base::kPayloadBits >= Header::kBits);
static_assert(Header::kBits == 4u);

inline constexpr size_t kPayloadBits = base::kPayloadBits - Header::kBits;

static_assert(kPayloadBits == 72u);
static_assert(kPayloadBits % 8u == 0u);

} // namespace smcp::proto::control
