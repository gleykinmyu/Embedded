#pragma once

#include <stdint.h>
#include "bitmask.hpp"
#include "phl_defs.hpp"

namespace REG {

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

    /// Прочитать бит с номером `bit` (0 = LSB). При bit вне [0, width) — false.
    bool readBit(uint8_t bit) const noexcept
    {
        constexpr uint8_t width = static_cast<uint8_t>(sizeof(Word) * 8U);
        if (bit >= width)
            return false;
        return (read() & static_cast<Word>(Word{1} << bit)) != Word{};
    }

    /// Записать бит с номером `bit` (0 = LSB): true → 1, false → 0.
    /// При bit вне [0, width) — no-op, возвращает false.
    bool writeBit(uint8_t bit, bool value) noexcept
    {
        constexpr uint8_t width = static_cast<uint8_t>(sizeof(Word) * 8U);
        if (bit >= width)
            return false;
        const Word mask = static_cast<Word>(Word{1} << bit);
        Word v = read();
        if (value)
            v = static_cast<Word>(v | mask);
        else
            v = static_cast<Word>(v & static_cast<Word>(~mask));
        write(v);
        return true;
    }
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
