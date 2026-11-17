/**
 * @file ringbuffer.hpp
 * @brief Типизированный кольцевой буфер (ёмкость = Size, через счётчик _size).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace MISC {

template <typename T, std::size_t Size>
class RingBuffer {
    static_assert(Size >= 1, "RingBuffer: Size >= 1");

    T _buffer[Size];
    volatile std::size_t _head = 0;
    volatile std::size_t _tail = 0;
    volatile std::size_t _size = 0;
    volatile std::size_t _ovfCount = 0;

public:
    [[nodiscard]] bool push(const T& item) noexcept
    {
        if (_size == Size) {
            ++_ovfCount;
            return false;
        }
        _buffer[_head] = item;
        _head = (_head + 1u) % Size;
        ++_size;
        return true;
    }

    [[nodiscard]] bool pop(T& item) noexcept
    {
        if (_size == 0u) {
            return false;
        }
        item = _buffer[_tail];
        _tail = (_tail + 1u) % Size;
        --_size;
        return true;
    }

    /** Указатель на голову (для retry без копии); nullptr если пусто. */
    [[nodiscard]] const T* peek() const noexcept
    {
        if (_size == 0u) {
            return nullptr;
        }
        return &_buffer[_tail];
    }

    /** Снять голову без копирования (после успешной обработки peek). */
    void drop() noexcept { drop(1u); }

    /** Снять `n` элементов с головы. */
    void drop(std::size_t n) noexcept
    {
        if (n > _size) {
            n = _size;
        }
        _tail = (_tail + n) % Size;
        _size -= n;
    }

    struct Linear {
        T* data;
        std::size_t n;
    };

    /** Занятые элементы одним или двумя линейными кусками (wrap). Без копии, до `drop`. */
    [[nodiscard]] uint8_t peekLinear(Linear out[2]) noexcept
    {
        const std::size_t n = _size;
        if (n == 0u) {
            return 0u;
        }
        const std::size_t tail = _tail;
        const std::size_t first = Size - tail;
        if (n <= first) {
            out[0] = {&_buffer[tail], n};
            return 1u;
        }
        out[0] = {&_buffer[tail], first};
        out[1] = {&_buffer[0], n - first};
        return 2u;
    }

    [[nodiscard]] std::size_t size() const noexcept { return _size; }

    [[nodiscard]] std::size_t space() const noexcept { return Size - _size; }

    [[nodiscard]] bool empty() const noexcept { return _size == 0u; }

    void clear() noexcept
    {
        _head = _tail = 0;
        _size = 0;
        _ovfCount = 0;
    }

    void clearData() noexcept
    {
        _head = _tail = 0;
        _size = 0;
    }

    [[nodiscard]] std::size_t overflows() const noexcept { return _ovfCount; }
    void clearOverflows() noexcept { _ovfCount = 0; }
};

} // namespace MISC
