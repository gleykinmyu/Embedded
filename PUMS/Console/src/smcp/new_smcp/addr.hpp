/**
 * @file new_smcp/addr.hpp
 * @brief smcp::addr_t — адрес узла для comm::Node (src/dst кадра).
 *
 * Консоли 0x01…0x0F, серверы 0x10…0xEF, broadcast 0xFF.
 * kNone и границы диапазонов — тоже addr_t: тип ещё не завершён внутри себя,
 * поэтому константы определены ниже класса.
 */

#pragma once

#include <cstdint>

namespace smcp {

struct addr_t {
    uint8_t v = 0;

    static const addr_t kNone;
    static const addr_t kBroadcast;
    static const addr_t kConsoleMin;
    static const addr_t kConsoleMax;
    static const addr_t kServerMin;
    static const addr_t kServerMax;

    constexpr addr_t() = default;
    constexpr addr_t(uint8_t x) noexcept : v(x) {}

    [[nodiscard]] constexpr bool accepts(addr_t dst) const noexcept
    {
        return dst == *this || dst == kBroadcast;
    }

    [[nodiscard]] constexpr bool isBroadcast() const noexcept { return *this == kBroadcast; }
    [[nodiscard]] constexpr bool isUnicast() const noexcept
    {
        return *this != kNone && !isBroadcast();
    }
    [[nodiscard]] constexpr bool isConsole() const noexcept
    {
        return v >= kConsoleMin.v && v <= kConsoleMax.v;
    }
    [[nodiscard]] constexpr bool isServer() const noexcept
    {
        return v >= kServerMin.v && v <= kServerMax.v;
    }

    friend constexpr bool operator==(addr_t a, addr_t b) noexcept { return a.v == b.v; }
    friend constexpr bool operator!=(addr_t a, addr_t b) noexcept { return a.v != b.v; }
};

inline constexpr addr_t addr_t::kNone{};
inline constexpr addr_t addr_t::kBroadcast{0xFFu};
inline constexpr addr_t addr_t::kConsoleMin{0x01u};
inline constexpr addr_t addr_t::kConsoleMax{0x0Fu};
inline constexpr addr_t addr_t::kServerMin{0x10u};
inline constexpr addr_t addr_t::kServerMax{0xEFu};

} // namespace smcp
