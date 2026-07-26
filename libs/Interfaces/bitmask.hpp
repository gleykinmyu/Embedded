#pragma once

namespace REG {

/**
 * Маска для enum class со значениями-битами. Хранит сырое слово; неявно строится из Enum.
 * Операторы |, &, ~ комбинируют маски. Enum|Enum для своего enum — в том же namespace, что и enum (ADL), см. REG_BITMASK_ENUM_OPS.
 */
template <typename Enum>
class BitMask {
    static_assert(__is_enum(Enum), "BitMask: Enum — enum class");
    using Word = __underlying_type(Enum);

    Word mask_ = 0;

    /// Только из комбинаций внутри класса; снаружи нельзя собрать маску из «голого» Word.
    constexpr explicit BitMask(Word w) noexcept : mask_(w) {}

public:
    constexpr BitMask() noexcept = default;

    constexpr BitMask(Enum e) noexcept : mask_(static_cast<Word>(e)) {}

    static constexpr BitMask from_raw(Word w) noexcept { return BitMask(w); }

    constexpr Word raw() const noexcept { return mask_; }

    constexpr bool any(BitMask m) const noexcept { return (mask_ & m.mask_) != Word{}; }

    constexpr bool all(BitMask m) const noexcept
    {
        const Word masked = static_cast<Word>(mask_ & m.mask_);
        return masked == m.mask_;
    }

    /// Включить биты из m (|=).
    void set(BitMask m) noexcept { mask_ = static_cast<Word>(mask_ | m.mask_); }

    /// Сбросить биты из m (&= ~m).
    void clear(BitMask m) noexcept
    {
        mask_ = static_cast<Word>(mask_ & static_cast<Word>(~m.raw()));
    }

    constexpr BitMask operator|(BitMask o) const noexcept
    {
        return BitMask(static_cast<Word>(mask_ | o.mask_));
    }

    friend constexpr BitMask operator|(Enum e, BitMask m) noexcept { return BitMask(e) | m; }

    constexpr BitMask operator&(BitMask o) const noexcept
    {
        return BitMask(static_cast<Word>(mask_ & o.mask_));
    }

    friend constexpr BitMask operator&(Enum e, BitMask m) noexcept { return BitMask(e) & m; }

    constexpr BitMask operator~() const noexcept { return BitMask(static_cast<Word>(~mask_)); }
};

} // namespace REG

/**
 * Раскрывать сразу после enum class в его namespace — иначе ADL не найдёт Enum|Enum (аргументы — только UART::SR и т.д.).
 */
#define REG_BITMASK_ENUM_OPS(Enum)                                                         \
    inline constexpr REG::BitMask<Enum> operator|(Enum a, Enum b) noexcept                \
    {                                                                                    \
        return REG::BitMask<Enum>(a) | REG::BitMask<Enum>(b);                            \
    }                                                                                    \
    inline constexpr REG::BitMask<Enum> operator&(Enum a, Enum b) noexcept               \
    {                                                                                    \
        return REG::BitMask<Enum>(a) & REG::BitMask<Enum>(b);                            \
    }
