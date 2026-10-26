/**
 * @file hub_can.hpp
 * @brief In-process broadcast ICAN: N портов, как smcp_bus.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ican.hpp"

namespace smcp {
namespace host {

class HubCan;

class CanHub {
public:
    static constexpr uint8_t kMaxPorts = 24u;

    void attach(HubCan& port) noexcept;
    void detach(HubCan& port) noexcept;
    void deliver(HubCan* from, const BIF::CAN::Frame& frame) noexcept;
    void inject(const BIF::CAN::Frame& frame) noexcept;

private:
    HubCan* _ports[kMaxPorts]{};
    uint8_t _n = 0;
};

class HubCan : public BIF::CAN::ICAN {
public:
    static constexpr std::size_t kRxDepth = 64u;

    explicit HubCan(CanHub& hub) noexcept
        : _hub(hub)
    {
        _hub.attach(*this);
    }

    ~HubCan() override { _hub.detach(*this); }

    HubCan(const HubCan&) = delete;
    HubCan& operator=(const HubCan&) = delete;

    void setMute(bool tx, bool rx) noexcept
    {
        _mute_tx = tx;
        _mute_rx = rx;
    }

    [[nodiscard]] bool pushRx(const BIF::CAN::Frame& frame) noexcept
    {
        if (_mute_rx) {
            return true;
        }
        if (_count >= kRxDepth) {
            _status = BIF::CAN::Status::OverFlowRX;
            return false;
        }
        _rx[_head] = frame;
        _head = (_head + 1u) % kRxDepth;
        ++_count;
        return true;
    }

    bool open(uint32_t bitrate) override
    {
        (void)bitrate;
        _open = true;
        _status = BIF::CAN::Status::OK;
        return true;
    }

    void close() override
    {
        _open = false;
        purge();
    }

    bool isOpen() override { return _open; }

    bool send(const BIF::CAN::Frame& frame) override
    {
        if (!_open) {
            return false;
        }
        if (_mute_tx) {
            return true;
        }
        _hub.deliver(this, frame);
        return true;
    }

    bool recv(BIF::CAN::Frame& out) override
    {
        if (!_open || _count == 0u) {
            return false;
        }
        out = _rx[_tail];
        _tail = (_tail + 1u) % kRxDepth;
        --_count;
        return true;
    }

    std::size_t available() const override { return _count; }

    std::size_t availableForWrite() const override { return _open ? 8u : 0u; }

    void purge() override
    {
        _head = 0;
        _tail = 0;
        _count = 0;
    }

    void purgeOutput() override {}
    void flush() override {}

    BIF::CAN::Status getStatus() override { return _status; }

    void clearErrors() override { _status = BIF::CAN::Status::OK; }

private:
    CanHub& _hub;
    bool _open = false;
    bool _mute_tx = false;
    bool _mute_rx = false;
    BIF::CAN::Status _status = BIF::CAN::Status::OK;
    BIF::CAN::Frame _rx[kRxDepth]{};
    std::size_t _head = 0;
    std::size_t _tail = 0;
    std::size_t _count = 0;
};

inline void CanHub::attach(HubCan& port) noexcept
{
    if (_n >= kMaxPorts) {
        return;
    }
    _ports[_n++] = &port;
}

inline void CanHub::detach(HubCan& port) noexcept
{
    for (uint8_t i = 0; i < _n; ++i) {
        if (_ports[i] != &port) {
            continue;
        }
        _ports[i] = _ports[_n - 1u];
        _ports[_n - 1u] = nullptr;
        --_n;
        return;
    }
}

inline void CanHub::deliver(HubCan* from, const BIF::CAN::Frame& frame) noexcept
{
    for (uint8_t i = 0; i < _n; ++i) {
        if (_ports[i] != nullptr && _ports[i] != from) {
            (void)_ports[i]->pushRx(frame);
        }
    }
}

inline void CanHub::inject(const BIF::CAN::Frame& frame) noexcept
{
    deliver(nullptr, frame);
}

} // namespace host
} // namespace smcp
