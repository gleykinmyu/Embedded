/**
 * @file comm/payload.hpp
 * @brief comm::Payload / BitPayload — буфер байт и битовое слово.
 *
 * Payload: push дописывает в конец, load(переменная, offset) читает байты.
 * BitPayload&lt;Bits&gt;: слово на Bits бит (uint8/16/32), поля от младших битов.
 *
 *   #include "comm/payload.hpp"   // -I …/libs
 *
 *   comm::Payload<0, 8> buf;
 *   buf.clear();
 *   buf.push(0x01);
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace comm {

namespace detail {

/** underlying_type только у enum: иначе подстановка push(uint8_t) — жёсткая ошибка. */
template <typename E, bool = std::is_enum_v<E>>
struct is_u8_enum : std::false_type {};

template <typename E>
struct is_u8_enum<E, true> : std::is_same<std::underlying_type_t<E>, uint8_t> {};

} // namespace detail

/**
 * Слово на @a Bits бит. Хранилище — меньший из uint8_t / uint16_t / uint32_t,
 * в который эта ширина влезает. Поля — от младших битов.
 * value — uint8_t, лишние биты срезаются маской ширины nbits.
 *
 *   comm::BitPayload&lt;12&gt; h;   // uint16_t
 *   h.push(0x3Fu, 5, 6);      // биты [10:5]
 *   uint8_t txn = 0;
 *   h.load(txn, 5, 6);
 */
 template <uint8_t Bits>
 class BitPayload {
     static_assert(Bits > 0u && Bits <= 32u, "BitPayload: 1..32 bits");
 
 public:
     using word_t = std::conditional_t<
         Bits <= 8u, uint8_t, std::conditional_t<Bits <= 16u, uint16_t, uint32_t>>;
     static constexpr uint8_t kBits = Bits;
 
     constexpr BitPayload() noexcept = default;
     explicit constexpr BitPayload(word_t bits) noexcept : _bits(bits) {}
 
     [[nodiscard]] constexpr word_t bits() const noexcept { return _bits; }
 
     /** Положить @a value в биты [offset, offset+nbits). */
     void push(uint8_t value, uint8_t offset, uint8_t nbits) noexcept
     {
         uint32_t m = 0;
         if (!field(offset, nbits, m)) {
             return;
         }
         const uint32_t placed = (static_cast<uint32_t>(value) & m) << offset;
         const uint32_t keep = ~(m << offset);
         _bits = static_cast<word_t>((static_cast<uint32_t>(_bits) & keep) | placed);
     }
 
     /** enum class : uint8_t — тот же push, значение срезается маской nbits. */
     template <typename E, std::enable_if_t<detail::is_u8_enum<E>::value, int> = 0>
     void push(E value, uint8_t offset, uint8_t nbits) noexcept
     {
         push(static_cast<uint8_t>(value), offset, nbits);
     }
 
     /** Снять nbits с @a offset в @a out. false — поле не влезает в слово. */
     [[nodiscard]] bool load(uint8_t& out, uint8_t offset, uint8_t nbits) const noexcept
     {
         uint32_t m = 0;
         if (!field(offset, nbits, m)) {
             return false;
         }
         out = static_cast<uint8_t>((static_cast<uint32_t>(_bits) >> offset) & m);
         return true;
     }
 
 private:
     static bool field(uint8_t offset, uint8_t nbits, uint32_t& mask) noexcept
     {
         const bool ok = nbits != 0u && nbits <= 8u
             && static_cast<unsigned>(offset) + nbits <= kBits;
         assert(ok);
         if (!ok) {
             return false;
         }
         mask = nbits == 8u ? 0xFFu : ((1u << nbits) - 1u);
         return true;
     }
 
     word_t _bits{};
 };
 
/**
 * Буфер байт фиксированной ёмкости и текущая длина.
 *
 * @tparam LenMin  начальное _len
 * @tparam LenMax  ёмкость _data
 */
template <uint8_t LenMin, uint8_t LenMax>
class Payload {
    static_assert(LenMax > 0u, "Payload: LenMax > 0");
    static_assert(LenMin <= LenMax, "Payload: LenMin <= LenMax");

public:
    static constexpr uint8_t kLenMin = LenMin;
    static constexpr uint8_t kLenMax = LenMax;

    [[nodiscard]] constexpr uint8_t len() const noexcept { return _len; }

    void clear() noexcept
    {
        _len = 0;
        std::memset(_data, 0, LenMax);
    }

    void push(uint8_t b) noexcept
    {
        assert(_len < LenMax);
        if (_len >= LenMax) {
            return;
        }
        _data[_len++] = b;
    }

    void push(const uint8_t* src, std::size_t n) noexcept
    {
        if (n == 0u) {
            return;
        }
        assert(src != nullptr && n <= static_cast<std::size_t>(LenMax - _len));
        if (src == nullptr || n > static_cast<std::size_t>(LenMax - _len)) {
            return;
        }
        std::memcpy(_data + _len, src, n);
        _len = static_cast<uint8_t>(_len + n);
    }

    /** enum class : uint8_t — тот же байт на проводе. */
    template <typename E, std::enable_if_t<detail::is_u8_enum<E>::value, int> = 0>
    void push(E v) noexcept
    {
        push(static_cast<uint8_t>(v));
    }

    /** Дописать байты другого Payload. Не влезло — не пишем. */
    template <uint8_t SrcMin, uint8_t SrcMax>
    void push(const Payload<SrcMin, SrcMax>& src) noexcept
    {
        push(src._data, src._len);
    }

    /** Слово BitPayload — little-endian, ширина хранилища (1, 2 или 4 байта). */
    template <uint8_t Bits>
    void push(const BitPayload<Bits>& src) noexcept
    {
        using word = typename BitPayload<Bits>::word_t;
        if constexpr (std::is_same_v<word, uint8_t>) {
            push(src.bits());
        } else if constexpr (std::is_same_v<word, uint16_t>) {
            pushU16(src.bits());
        } else {
            pushU32(src.bits());
        }
    }

    void pushU16(uint16_t v) noexcept
    {
        assert(static_cast<uint8_t>(LenMax - _len) >= 2u);
        if (static_cast<uint8_t>(LenMax - _len) < 2u) {
            return;
        }
        push(static_cast<uint8_t>(v));
        push(static_cast<uint8_t>(v >> 8));
    }

    void pushU32(uint32_t v) noexcept
    {
        assert(static_cast<uint8_t>(LenMax - _len) >= 4u);
        if (static_cast<uint8_t>(LenMax - _len) < 4u) {
            return;
        }
        pushU16(static_cast<uint16_t>(v));
        pushU16(static_cast<uint16_t>(v >> 16));
    }

    /** Байт в @a out с @a offset. false — вне [0, len). _len не меняется. */
    [[nodiscard]] bool load(uint8_t& out, uint8_t offset) const noexcept
    {
        if (offset >= _len) {
            return false;
        }
        out = _data[offset];
        return true;
    }

    /** enum class : uint8_t. false — вне [0, len), @a out не меняется. */
    template <typename E, std::enable_if_t<detail::is_u8_enum<E>::value, int> = 0>
    [[nodiscard]] bool load(E& out, uint8_t offset) const noexcept
    {
        uint8_t raw = 0;
        if (!load(raw, offset)) {
            return false;
        }
        out = static_cast<E>(raw);
        return true;
    }

    [[nodiscard]] bool load(uint16_t& out, uint8_t offset) const noexcept
    {
        if (offset >= _len || static_cast<uint8_t>(_len - offset) < 2u) {
            return false;
        }
        const uint8_t lo = _data[offset];
        const uint8_t hi = _data[static_cast<uint8_t>(offset + 1u)];
        out = static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));
        return true;
    }

    [[nodiscard]] bool load(uint32_t& out, uint8_t offset) const noexcept
    {
        if (offset >= _len || static_cast<uint8_t>(_len - offset) < 4u) {
            return false;
        }
        uint16_t lo = 0;
        uint16_t hi = 0;
        if (!load(lo, offset) || !load(hi, static_cast<uint8_t>(offset + 2u))) {
            return false;
        }
        out = static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 16);
        return true;
    }

    /** Слово BitPayload с @a offset. false — не влезло, @a out не меняется. */
    template <uint8_t Bits>
    [[nodiscard]] bool load(BitPayload<Bits>& out, uint8_t offset) const noexcept
    {
        using word = typename BitPayload<Bits>::word_t;
        word raw{};
        if (!load(raw, offset)) {
            return false;
        }
        out = BitPayload<Bits>{raw};
        return true;
    }

    /** @a n байт в @a dst с @a offset. false — вне префикса. */
    [[nodiscard]] bool load(uint8_t* dst, std::size_t n, uint8_t offset) const noexcept
    {
        if (n == 0u) {
            return true;
        }
        if (dst == nullptr || offset >= _len || n > static_cast<std::size_t>(_len - offset)) {
            return false;
        }
        std::memcpy(dst, _data + offset, n);
        return true;
    }

    /** Байты с @a offset до конца — в @a dst. false — не влезло, @a dst не меняется. */
    template <uint8_t DstMin, uint8_t DstMax>
    [[nodiscard]] bool load(Payload<DstMin, DstMax>& dst, uint8_t offset) const noexcept
    {
        if (offset > _len) {
            return false;
        }
        const uint8_t n = static_cast<uint8_t>(_len - offset);
        if (n > DstMax) {
            return false;
        }
        std::memset(dst._data, 0, DstMax);
        if (n != 0u) {
            std::memcpy(dst._data, _data + offset, n);
        }
        dst._len = n;
        return true;
    }

private:
    template <uint8_t, uint8_t>
    friend class Payload;

    uint8_t _len = LenMin;
    uint8_t _data[LenMax]{};
};

} // namespace comm
