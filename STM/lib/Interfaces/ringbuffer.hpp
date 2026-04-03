#pragma once
#include <stdint.h>
#include <stddef.h>

// =================================================================
// КОЛЬЦЕВОЙ БУФЕР
// =================================================================

template <size_t Size> 
class RingBuffer 
{
  uint8_t _buffer[Size];
  // Указатель на голову буфера
  volatile size_t _head = 0;
  // Указатель на хвост буфера
  volatile size_t _tail = 0;
  // Счетчик переполнений
  volatile size_t _ovfCount = 0;

public:

// Добавление элемента в буфер
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

  // Извлечение элемента из буфера
  bool pop(uint8_t &data) {
    if (_head == _tail)
      return false;
    data = _buffer[_tail];
    _tail = (_tail + 1) % Size;
    return true;
  }

  // Количество элементов в буфере
  size_t size() const {
    return (_head >= _tail) ? (_head - _tail) : (Size - _tail + _head);
  }

  // Свободное место в буфере
  size_t space() const { return (Size - 1) - size(); }

  // Очистка буфера
  void clear() {
    _head = _tail = 0;
    _ovfCount = 0;
  }

  // Количество переполнений буфера
  size_t overflows() const { return _ovfCount; }
  // Очистка счетчика переполнений
  void clearOverflows() { _ovfCount = 0; }
};
