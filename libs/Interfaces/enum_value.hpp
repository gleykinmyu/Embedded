#pragma once

#include <cstdint>
#include <type_traits>

/**
 * @file enum_value.hpp
 *
 * EnumValue<Enum> — плотный enum class с First / Last.
 * Last — автономерной sentinel (как бывший Count): первый свободный код после
 * значимых членов. Диапазон значений: [First, Last).
 *
 * EnumValueChain<Enum, ParentValue>: слой над наследником EnumValue.
 * Child::First == ParentEnum::Last (без +1).
 * isOwn() — raw в своём [First, Last); у Chain ещё isParent().
 *
 *   enum class Transport : uint8_t {
 *       First = 0, None = First, Busy, Body, Last // Last == Body+1
 *   };
 *   struct TErr : EnumValue<Transport, TErr> { … };
 *   enum class App : uint8_t {
 *       First = TErr::Last, // Word-константа родителя
 *       Limits = First, …, Last
 *   };
 *   class AppErr : public EnumValueChain<App, TErr> { … };
 *
 * @tparam Derived CRTP: from_raw() возвращает Derived (void → сам EnumValue).
 */

namespace MISC {
namespace detail {

template <typename Enum>
struct enum_bounds {
    static_assert(std::is_enum_v<Enum>, "enum_bounds: Enum must be an enum");
    using Word = std::underlying_type_t<Enum>;

    static constexpr Word first() noexcept
    {
        return static_cast<Word>(Enum::First);
    }
    static constexpr Word last() noexcept
    {
        return static_cast<Word>(Enum::Last);
    }
    /** Число значимых кодов [First, Last). */
    static constexpr Word size() noexcept { return last() - first(); }

    static_assert(first() < last(), "enum: First < Last (Last — exclusive end)");
};

} // namespace detail

template <typename Enum, typename Derived = void>
class EnumValue {
    static_assert(std::is_enum_v<Enum>, "EnumValue: Enum must be an enum");

    using Bounds = detail::enum_bounds<Enum>;
    using Self = std::conditional_t<std::is_void_v<Derived>, EnumValue, Derived>;

protected:
    using Word = std::underlying_type_t<Enum>;
    Word raw_{};

    explicit constexpr EnumValue(Word w) noexcept : raw_(w) {}

    constexpr void set_raw(Word w) noexcept { raw_ = w; }

public:
    using enum_type = Enum;
    using word_type = Word;

    static constexpr Word First = Bounds::first();
    static constexpr Word Last = Bounds::last();
    static constexpr Word Size = Bounds::size();

    constexpr EnumValue() noexcept = default;
    constexpr EnumValue(Enum e) noexcept : raw_(static_cast<Word>(e)) {}

    [[nodiscard]] static constexpr Self from_raw(Word w) noexcept
    {
        Self s{};
        s.set_raw(w);
        return s;
    }

    /** Свой слой: First ≤ raw < Last. */
    [[nodiscard]] constexpr bool isOwn() const noexcept
    {
        return raw_ >= First && raw_ < Last;
    }

    /** Синоним isOwn() для плотного enum без дыр. */
    [[nodiscard]] constexpr bool isValid() const noexcept { return isOwn(); }

    [[nodiscard]] constexpr Word raw() const noexcept { return raw_; }

    constexpr operator Enum() const noexcept { return static_cast<Enum>(raw_); }

    [[nodiscard]] constexpr explicit operator uint8_t() const noexcept
    {
        return static_cast<uint8_t>(raw_);
    }

    constexpr EnumValue& operator=(Enum e) noexcept
    {
        raw_ = static_cast<Word>(e);
        return *this;
    }

    friend constexpr bool operator==(EnumValue a, EnumValue b) noexcept
    {
        return a.raw_ == b.raw_;
    }
    friend constexpr bool operator!=(EnumValue a, EnumValue b) noexcept
    {
        return !(a == b);
    }
    friend constexpr bool operator==(EnumValue a, Enum b) noexcept
    {
        return a.raw_ == static_cast<Word>(b);
    }
    friend constexpr bool operator==(Enum a, EnumValue b) noexcept
    {
        return b == a;
    }
    friend constexpr bool operator!=(EnumValue a, Enum b) noexcept
    {
        return !(a == b);
    }
    friend constexpr bool operator!=(Enum a, EnumValue b) noexcept
    {
        return !(a == b);
    }
};

/**
 * Слой над @a ParentValue (: EnumValue<ParentEnum>). Child::First == ParentEnum::Last.
 */
template <typename Enum, typename ParentValue>
class EnumValueChain : public ParentValue {
    static_assert(std::is_enum_v<Enum>, "EnumValueChain: Enum must be an enum");

    using ParentEnum = typename ParentValue::enum_type;
    using Base = ParentValue;
    using Word = std::underlying_type_t<Enum>;
    using Bounds = detail::enum_bounds<Enum>;
    using ParentBounds = detail::enum_bounds<ParentEnum>;

    static_assert(std::is_base_of_v<EnumValue<ParentEnum>, ParentValue>
                      || std::is_base_of_v<EnumValue<ParentEnum, ParentValue>, ParentValue>,
                  "EnumValueChain: ParentValue must derive from EnumValue<ParentEnum[, ParentValue]>");
    static_assert(std::is_same_v<Word, typename Base::word_type>,
                  "EnumValueChain: Enum and ParentEnum must share underlying type");
    static_assert(Bounds::first() == ParentBounds::last(),
                  "EnumValueChain: Enum::First must equal Parent::Last");

    explicit constexpr EnumValueChain(Word w) noexcept
        : Base()
    {
        this->set_raw(w);
    }

public:
    using enum_type = Enum;
    using parent_enum_type = ParentEnum;
    using parent_value_type = ParentValue;
    using word_type = Word;
    using Base::raw;

    static constexpr Word First = Bounds::first();
    static constexpr Word Last = Bounds::last();
    static constexpr Word Size = Bounds::size();

    constexpr EnumValueChain() noexcept = default;

    constexpr EnumValueChain(Enum e) noexcept
        : Base()
    {
        this->set_raw(static_cast<Word>(e));
    }

    constexpr EnumValueChain(ParentEnum e) noexcept
        : Base(e)
    {}

    constexpr EnumValueChain(const Base& parent) noexcept
        : Base(parent)
    {}

    [[nodiscard]] static constexpr EnumValueChain from_raw(Word w) noexcept
    {
        return EnumValueChain(w);
    }

    /** Свой [First, Last). Ниже First — isValid() родителя, по цепочке до корня. */
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        if (this->raw() < First) {
            return Base::isValid();
        }
        return this->raw() < Last;
    }

    /** Свой (child) слой: Enum::First ≤ raw < Enum::Last. */
    [[nodiscard]] constexpr bool isOwn() const noexcept
    {
        return this->raw() >= First && this->raw() < Last;
    }

    /** raw в [Parent::First, Parent::Last). */
    [[nodiscard]] constexpr bool isParent() const noexcept
    {
        return this->raw() >= ParentBounds::first()
            && this->raw() < ParentBounds::last();
    }

    constexpr operator Enum() const noexcept
    {
        return static_cast<Enum>(this->raw());
    }

    constexpr EnumValueChain& operator=(Enum e) noexcept
    {
        this->set_raw(static_cast<Word>(e));
        return *this;
    }

    constexpr EnumValueChain& operator=(ParentEnum e) noexcept
    {
        Base::operator=(e);
        return *this;
    }

    constexpr EnumValueChain& operator=(const Base& parent) noexcept
    {
        Base::operator=(parent);
        return *this;
    }

    friend constexpr bool operator==(EnumValueChain a, EnumValueChain b) noexcept
    {
        return a.raw() == b.raw();
    }
    friend constexpr bool operator!=(EnumValueChain a, EnumValueChain b) noexcept
    {
        return !(a == b);
    }
    friend constexpr bool operator==(EnumValueChain a, Enum b) noexcept
    {
        return a.raw() == static_cast<Word>(b);
    }
    friend constexpr bool operator==(Enum a, EnumValueChain b) noexcept
    {
        return b == a;
    }
    friend constexpr bool operator!=(EnumValueChain a, Enum b) noexcept
    {
        return !(a == b);
    }
    friend constexpr bool operator!=(Enum a, EnumValueChain b) noexcept
    {
        return !(a == b);
    }
    friend constexpr bool operator==(EnumValueChain a, ParentEnum b) noexcept
    {
        return a.raw() == static_cast<Word>(b);
    }
    friend constexpr bool operator==(ParentEnum a, EnumValueChain b) noexcept
    {
        return b == a;
    }
    friend constexpr bool operator!=(EnumValueChain a, ParentEnum b) noexcept
    {
        return !(a == b);
    }
    friend constexpr bool operator!=(ParentEnum a, EnumValueChain b) noexcept
    {
        return !(a == b);
    }
};

} // namespace MISC
