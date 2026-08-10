/**
 * @file ringbuffer.hpp
 * @brief Типизированный кольцевой буфер (1 слот всегда свободен).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace MISC {

template <typename T, std::size_t Size>
class RingBuffer {
    static_assert(Size >= 2, "RingBuffer: Size >= 2");

    T _buffer[Size];
    volatile std::size_t _head = 0;
    volatile std::size_t _tail = 0;
    volatile std::size_t _ovfCount = 0;

public:
    [[nodiscard]] bool push(const T& item) noexcept
    {
        const std::size_t next = (_head + 1u) % Size;
        if (next == _tail) {
            ++_ovfCount;
            return false;
        }
        _buffer[_head] = item;
        _head = next;
        return true;
    }

    [[nodiscard]] bool pop(T& item) noexcept
    {
        if (_head == _tail) {
            return false;
        }
        item = _buffer[_tail];
        _tail = (_tail + 1u) % Size;
        return true;
    }

    /** Указатель на голову (для retry без копии); nullptr если пусто. */
    [[nodiscard]] const T* peek() const noexcept
    {
        if (_head == _tail) {
            return nullptr;
        }
        return &_buffer[_tail];
    }

    /** Снять голову без копирования (после успешной обработки peek). */
    void drop() noexcept
    {
        if (_head != _tail) {
            _tail = (_tail + 1u) % Size;
        }
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return (_head >= _tail) ? (_head - _tail) : (Size - _tail + _head);
    }

    [[nodiscard]] std::size_t space() const noexcept { return (Size - 1u) - size(); }

    [[nodiscard]] bool empty() const noexcept { return _head == _tail; }

    void clear() noexcept
    {
        _head = _tail = 0;
        _ovfCount = 0;
    }

    void clearData() noexcept { _head = _tail = 0; }

    [[nodiscard]] std::size_t overflows() const noexcept { return _ovfCount; }
    void clearOverflows() noexcept { _ovfCount = 0; }
};

} // namespace MISC
