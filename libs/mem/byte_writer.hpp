/**
 * @file mem/byte_writer.hpp
 * @brief mem::IByteWriter / IByteReader / ByteBuffer.
 *
 * IByteWriter — ISlot&lt;uint8_t&gt; + cursor: push/clear (запись).
 * IByteReader — тот же ISlot&lt;uint8_t&gt; + cursor: pull (чтение, методы const).
 * ByteBuffer  — Store с базой IByteWriter (владеет uint8_t[N]).
 */
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "mem/slot.hpp"

namespace mem {

class IByteWriter : public ISlot<uint8_t, std::size_t> {
    using Base = ISlot<uint8_t, std::size_t>;

public:
    using typename Base::cap_t;
    using typename Base::idx_t;
    using Base::Base;
    using Base::capacity;

    IByteWriter() noexcept : Base(nullptr, 0) {}

    explicit IByteWriter(Base& slots) noexcept : Base(slots.data(), slots.capacity()) {}

    [[nodiscard]] cap_t cursor() const noexcept { return _cursor; }

    void clear() noexcept
    {
        _cursor = 0;
        uint8_t* d = data();
        const cap_t cap = capacity();
        if (d != nullptr && cap != 0u) {
            std::memset(d, 0, cap);
        }
    }

    bool push(uint8_t b) noexcept
    {
        uint8_t* d = data();
        assert(d != nullptr && _cursor < capacity());
        if (d == nullptr || _cursor >= capacity()) {
            return false;
        }
        d[_cursor++] = b;
        return true;
    }

    bool push(const uint8_t* src, std::size_t n) noexcept
    {
        if (n == 0u) {
            return true;
        }
        uint8_t* d = data();
        if (src == nullptr || d == nullptr || capacity() - _cursor < n) {
            return false;
        }
        std::memcpy(d + _cursor, src, n);
        _cursor += n;
        return true;
    }

    /** enum class : uint8_t — тот же байт на проводе. */
    template <typename E>
    auto push(E v) noexcept -> std::enable_if_t<
        std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, uint8_t>, bool>
    {
        return push(static_cast<uint8_t>(v));
    }

    bool pushU16(uint16_t v) noexcept
    {
        if (data() == nullptr || capacity() - _cursor < 2u) {
            return false;
        }
        return push(static_cast<uint8_t>(v))
            && push(static_cast<uint8_t>(v >> 8));
    }

    bool pushU32(uint32_t v) noexcept
    {
        if (data() == nullptr || capacity() - _cursor < 4u) {
            return false;
        }
        return pushU16(static_cast<uint16_t>(v))
            && pushU16(static_cast<uint16_t>(v >> 16));
    }

private:
    cap_t _cursor = 0;
};

class IByteReader : public ISlot<uint8_t, std::size_t> {
    using Base = ISlot<uint8_t, std::size_t>;

public:
    using typename Base::cap_t;
    using typename Base::idx_t;
    using Base::Base;
    using Base::capacity;

    IByteReader() noexcept : Base(nullptr, 0) {}

    explicit IByteReader(Base& slots) noexcept
        : Base(slots.data(), slots.capacity())
        , _cursor(this->capacity())
    {}

    [[nodiscard]] const uint8_t* data() const noexcept { return Base::data(); }

    [[nodiscard]] cap_t cursor() const noexcept { return _cursor; }

    bool pull(uint8_t* dst, std::size_t n) const noexcept
    {
        if (n == 0u) {
            return true;
        }
        const uint8_t* d = data();
        if (dst == nullptr || d == nullptr || _cursor < n) {
            return false;
        }
        _cursor -= n;
        std::memcpy(dst, d + _cursor, n);
        return true;
    }

    [[nodiscard]] uint8_t pull() const noexcept
    {
        const uint8_t* d = data();
        if (d == nullptr || _cursor == 0u) {
            return 0;
        }
        return d[--_cursor];
    }

    [[nodiscard]] uint16_t pullU16() const noexcept
    {
        if (data() == nullptr || _cursor < 2u) {
            return 0;
        }
        const uint8_t hi = pull();
        const uint8_t lo = pull();
        return static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));
    }

    [[nodiscard]] uint32_t pullU32() const noexcept
    {
        if (data() == nullptr || _cursor < 4u) {
            return 0;
        }
        const uint16_t hi = pullU16();
        const uint16_t lo = pullU16();
        return static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 16);
    }

private:
    mutable cap_t _cursor = 0;
};

template <std::size_t Capacity>
using ByteBuffer = Store<uint8_t, std::size_t, Capacity, IByteWriter>;

} // namespace mem
