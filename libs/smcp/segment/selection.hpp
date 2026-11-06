/**
 * @file selection.hpp
 * @brief segment::Selection — маска осей 0..31 на одном server_id.
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

namespace smcp {
namespace segment {

/** Максимум осей на один server ID (бит Selection). Inventory — MaxAxes у Server/Console. */
inline constexpr uint8_t kMechCount = 32u;

/** Выбор штанкетов 0..31 — бит N означает «механизм N включён» (один server ID). */
class Selection {
public:
    constexpr Selection() noexcept = default;

    /** Собрать маску из сырого uint32. */
    [[nodiscard]] static constexpr Selection from_raw(uint32_t raw) noexcept
    {
        return Selection(raw);
    }

    [[nodiscard]] constexpr uint32_t raw() const noexcept { return bits_; }
    [[nodiscard]] constexpr operator uint32_t() const noexcept { return bits_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return bits_ == 0u; }
    [[nodiscard]] constexpr bool any() const noexcept { return !empty(); }

    /** Ось @a id входит в маску. */
    [[nodiscard]] constexpr bool contains(uint8_t id) const noexcept
    {
        return id < kMechCount && ((bits_ >> id) & 1u) != 0u;
    }

    /** Число установленных бит. */
    [[nodiscard]] constexpr uint8_t count() const noexcept
    {
        uint8_t n = 0;
        for (uint32_t v = bits_; v != 0u; v >>= 1) {
            n = static_cast<uint8_t>(n + (v & 1u));
        }
        return n;
    }

    /**
     * Обход установленных бит: fn(index, axis_id).
     * @a index — 0…count()-1 только по включённым id (не phys 0…31).
     * Если fn → bool, true — стоп.
     */
    template <typename Fn>
    constexpr void forEach(Fn&& fn) const
        noexcept(noexcept(fn(std::declval<uint8_t>(), std::declval<uint8_t>())))
    {
        uint8_t index = 0u;
        for (uint8_t id = 0u; id < kMechCount; ++id) {
            if (((bits_ >> id) & 1u) == 0u) {
                continue;
            }
            if constexpr (std::is_invocable_r_v<bool, Fn&, uint8_t, uint8_t>) {
                if (fn(index, id)) {
                    return;
                }
            } else {
                fn(index, id);
            }
            ++index;
        }
    }

    /** Включить бит оси; id вне 0..31 — no-op. */
    constexpr void add(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ |= (1u << id);
        }
    }

    /** Маска из одной оси. */
    [[nodiscard]] static constexpr Selection of(uint8_t id) noexcept
    {
        Selection s{};
        s.add(id);
        return s;
    }
    /** Снять бит оси; id вне 0..31 — no-op. */
    constexpr void remove(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ &= ~(1u << id);
        }
    }
    /** Инвертировать бит оси; id вне 0..31 — no-op. */
    constexpr void toggle(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ ^= (1u << id);
        }
    }

    constexpr Selection operator|(Selection o) const noexcept { return Selection(bits_ | o.bits_); }
    constexpr Selection operator&(Selection o) const noexcept { return Selection(bits_ & o.bits_); }
    constexpr Selection operator~() const noexcept { return Selection(~bits_); }

private:
    constexpr explicit Selection(uint32_t raw) noexcept : bits_(raw) {}

    uint32_t bits_ = 0;
};

static_assert(sizeof(Selection) == sizeof(uint32_t));

} // namespace segment
} // namespace smcp
