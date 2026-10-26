/**
 * @file clock.hpp
 * @brief Host clockMs() для Node (steady_clock → ms).
 */

#pragma once

#include <chrono>
#include <cstdint>

namespace smcp {
namespace host {

inline uint32_t clockMs() noexcept
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0);
    return static_cast<uint32_t>(ms.count());
}

} // namespace host
} // namespace smcp
