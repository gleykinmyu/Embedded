/**
 * @file packet_queue.hpp
 * @brief Кольцевая очередь smcp::msg::Packet (между CAN/IRQ и MConsole/MServer).
 */

#pragma once

#include <cstddef>

#include "message.hpp"

namespace smcp {
namespace msg {

template <std::size_t Capacity>
class PacketQueue {
    static_assert(Capacity >= 2, "PacketQueue: Capacity >= 2");

    Packet _buf[Capacity];
    volatile std::size_t _head = 0;
    volatile std::size_t _tail = 0;
    volatile std::size_t _ovf = 0;

public:
    [[nodiscard]] bool push(const Packet& pkt) noexcept
    {
        const std::size_t next = (_head + 1) % Capacity;
        if (next == _tail) {
            ++_ovf;
            return false;
        }
        _buf[_head] = pkt;
        _head = next;
        return true;
    }

    [[nodiscard]] bool pop(Packet& pkt) noexcept
    {
        if (_head == _tail) {
            return false;
        }
        pkt = _buf[_tail];
        _tail = (_tail + 1) % Capacity;
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        if (_head >= _tail) {
            return _head - _tail;
        }
        return Capacity - _tail + _head;
    }

    [[nodiscard]] std::size_t space() const noexcept
    {
        return Capacity - 1u - size();
    }

    [[nodiscard]] bool empty() const noexcept { return _head == _tail; }

    [[nodiscard]] std::size_t overflows() const noexcept { return _ovf; }
    void clearOverflows() noexcept { _ovf = 0; }

    void clear() noexcept
    {
        _head = 0;
        _tail = 0;
    }
};

} // namespace msg
} // namespace smcp
