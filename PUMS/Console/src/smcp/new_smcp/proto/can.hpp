/**
 * @file new_smcp/proto/can.hpp
 * @brief Битовый бюджет классического CAN. Это ограничение формата, не драйвер.
 */

#pragma once

#include <cstddef>

namespace smcp::proto {

struct CanLimits {
    static constexpr size_t kIdBits = 29;
    static constexpr size_t kDataMax = 8;
    static constexpr size_t kDataBits = kDataMax * 8;
    static constexpr size_t kFrameBits = kIdBits + kDataBits;
};

static_assert(CanLimits::kFrameBits == 93u);

} // namespace smcp::proto
