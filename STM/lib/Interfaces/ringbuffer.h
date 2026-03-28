#pragma once
#include <cstdint>
#include <cstddef>

// =================================================================
// КОЛЬЦЕВОЙ БУФЕР
// Легковесная структура данных для прерываний
// =================================================================
template <size_t Size>
class RingBuffer 
{
    uint8_t _buffer[Size];
    volatile size_t _head = 0;
    volatile size_t _tail = 0;
    volatile size_t _ovfCount = 0;

public:
    bool push(uint8_t data) {
        size_t next = (_head + 1) % Size;
        if (next == _tail) {
            _ovfCount++;
            return false;
        }
        _buffer[_head] = data;
        _head = next;
        return true;
    }

    bool pop(uint8_t& data) {
        if (_head == _tail) return false;
        data = _buffer[_tail];
        _tail = (_tail + 1) % Size;
        return true;
    }

    size_t size() const { return (_head >= _tail) ? (_head - _tail) : (Size - _tail + _head); }
    size_t space() const { return (Size - 1) - size(); }
    void   clear() { _head = _tail = 0; _ovfCount = 0; }
    size_t overflows() const { return _ovfCount; }
    void   clearOverflows() { _ovfCount = 0; }
};
