#pragma once

#include <stdint.h>
#include "phl_defs.hpp"

namespace REG {

template <typename Enum, PHL::ID PeriphId, size_t RegByteOffset>
class PropertyBits;

/**
 * Маска для enum class со значениями-битами. Хранит сырое слово; неявно строится из Enum.
 * Операторы |, &, ~ комбинируют маски. Enum|Enum для своего enum — в том же namespace, что и enum (ADL), см. REG_BITMASK_ENUM_OPS.
 */
template <typename Enum>
class BitMask {
    /// PropertyBits::get вызывает from_raw; узкий friend на старых GCC внутри класса не оформить.
    template <typename E, PHL::ID I, size_t O>
    friend class PropertyBits;

    static_assert(__is_enum(Enum), "BitMask: Enum — enum class");
    using Word = __underlying_type(Enum);

    Word mask_ = 0;

    /// Только из комбинаций внутри класса; снаружи нельзя собрать маску из «голого» Word.
    constexpr explicit BitMask(Word w) noexcept : mask_(w) {}

    static constexpr BitMask from_raw(Word w) noexcept { return BitMask(w); }

public:
    constexpr BitMask() noexcept = default;

    constexpr BitMask(Enum e) noexcept : mask_(static_cast<Word>(e)) {}

    constexpr Word raw() const noexcept { return mask_; }

    constexpr bool any(BitMask m) const noexcept { return (mask_ & m.raw()) != Word{}; }

    constexpr bool any(Enum e) const noexcept { return any(BitMask(e)); }

    constexpr bool all(BitMask m) const noexcept
    {
        const Word masked = static_cast<Word>(mask_ & m.raw());
        return masked == m.raw();
    }

    constexpr bool all(Enum e) const noexcept { return all(BitMask(e)); }

    /// Включить биты из m (|=).
    void set(BitMask m) noexcept { mask_ = static_cast<Word>(mask_ | m.mask_); }

    void set(Enum e) noexcept { mask_ = static_cast<Word>(mask_ | static_cast<Word>(e)); }

    /// Сбросить биты из m (&= ~m).
    void clear(BitMask m) noexcept
    {
        mask_ = static_cast<Word>(mask_ & static_cast<Word>(~m.raw()));
    }

    void clear(Enum e) noexcept
    {
        mask_ = static_cast<Word>(mask_ & static_cast<Word>(~static_cast<Word>(e)));
    }

    constexpr BitMask operator|(BitMask o) const noexcept
    {
        return BitMask(static_cast<Word>(mask_ | o.mask_));
    }

    constexpr BitMask operator|(Enum e) const noexcept { return *this | BitMask(e); }

    friend constexpr BitMask operator|(Enum e, BitMask m) noexcept { return BitMask(e) | m; }

    constexpr BitMask operator&(BitMask o) const noexcept
    {
        return BitMask(static_cast<Word>(mask_ & o.mask_));
    }

    constexpr BitMask operator&(Enum e) const noexcept { return *this & BitMask(e); }

    friend constexpr BitMask operator&(Enum e, BitMask m) noexcept { return BitMask(e) & m; }

    constexpr BitMask operator~() const noexcept { return BitMask(static_cast<Word>(~mask_)); }
};

/**
 * Слово регистра: только read / write / addr.
 * База — PHL::ID, смещение поля в байтах.
 */
template <PHL::ID PeriphId, size_t RegByteOffset, typename Word = uint32_t>
class PropertyWord {
    static_assert(sizeof(Word) == 1U || sizeof(Word) == 2U || sizeof(Word) == 4U,
                  "PropertyWord: Word 1 / 2 / 4 байта");

public:
    static volatile Word* addr() noexcept
    {
        return reinterpret_cast<volatile Word*>(static_cast<uintptr_t>(PeriphId) + RegByteOffset);
    }

    Word read() const noexcept { return *addr(); }

    void write(Word value) noexcept { *addr() = value; }
};

/**
 * Регистр с битовой моделью через BitMask<Enum>. any/all только для BitMask (enum неявно к ней приводится).
 */
template <typename Enum, PHL::ID PeriphId, size_t RegByteOffset>
class PropertyBits : public PropertyWord<PeriphId, RegByteOffset, __underlying_type(Enum)> {
    static_assert(__is_enum(Enum), "PropertyBits: Enum — enum class");
    using Word = __underlying_type(Enum);
    using Base = PropertyWord<PeriphId, RegByteOffset, Word>;
    using Mask = BitMask<Enum>;

public:
    using Base::write;
    using Base::addr;

    /// Сырое слово регистра (как PropertyWord::read).
    Word read_raw() const noexcept { return Base::read(); }

    /// Значение регистра как BitMask.
    Mask read() const noexcept { return Mask::from_raw(read_raw()); }

    /// Записать регистр целиком из маски (то же, что write(m.raw())).
    void write(Mask m) noexcept { Base::write(static_cast<Word>(m.raw())); }

    bool any(Mask m) const noexcept { return (read_raw() & m.raw()) != Word{}; }

    bool all(Mask m) const noexcept
    {
        const Word v = read_raw();
        return (v & m.raw()) == m.raw();
    }

    /// То же, что read() (один read регистра).
    Mask get() const noexcept { return read(); }

    void set(Mask m) noexcept { Base::write(static_cast<Word>(read_raw() | m.raw())); }

    void clear(Mask m) noexcept
    {
        Base::write(static_cast<Word>(read_raw() & static_cast<Word>(~m.raw())));
    }
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
