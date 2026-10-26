#pragma once

#include <cstdint>
#include <type_traits>

/**
 * @file enum_value.hpp
 *
 * Обёртка над enum class с последним членом `Count`.
 * Значения 0 … Count-1 считаются валидными (плотный диапазон).
 */

namespace MISC {

template <typename Enum>
class EnumValue {
    static_assert(std::is_enum_v<Enum>, "EnumValue: Enum must be an enum");
    using Word = std::underlying_type_t<Enum>;

    Enum value_{};

public:
    inline constexpr EnumValue() noexcept = default;
    inline constexpr EnumValue(Enum e) noexcept : value_(e) {}

    [[nodiscard]] static inline constexpr EnumValue from_raw(Word w) noexcept
    {
        return EnumValue(static_cast<Enum>(w));
    }

    /** true, если 0 ≤ raw < Enum::Count. */
    [[nodiscard]] inline constexpr bool isValid() const noexcept
    {
        return static_cast<Word>(value_) < static_cast<Word>(Enum::Count);
    }

    [[nodiscard]] inline constexpr Word raw() const noexcept
    {
        return static_cast<Word>(value_);
    }

    inline constexpr operator Enum() const noexcept { return value_; }

    inline constexpr EnumValue& operator=(Enum e) noexcept
    {
        value_ = e;
        return *this;
    }

    friend inline constexpr bool operator==(EnumValue a, EnumValue b) noexcept
    {
        return a.value_ == b.value_;
    }
    friend inline constexpr bool operator!=(EnumValue a, EnumValue b) noexcept
    {
        return !(a == b);
    }
    friend inline constexpr bool operator==(EnumValue a, Enum b) noexcept
    {
        return a.value_ == b;
    }
    friend inline constexpr bool operator==(Enum a, EnumValue b) noexcept
    {
        return a == b.value_;
    }
    friend inline constexpr bool operator!=(EnumValue a, Enum b) noexcept
    {
        return !(a == b);
    }
    friend inline constexpr bool operator!=(Enum a, EnumValue b) noexcept
    {
        return !(a == b);
    }
};

} // namespace MISC
