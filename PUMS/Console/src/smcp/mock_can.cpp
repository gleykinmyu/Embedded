/**
 * @file mock_can.cpp
 * @brief Реализация MockCan.
 */

#include "smcp/mock_can.hpp"

namespace smcp {

void MockCan::connect(MockCan& peer) noexcept
{
    if (this == &peer) {
        return;
    }
    _peer = &peer;
    peer._peer = this;
}

bool MockCan::open(uint32_t bitrate)
{
    (void)bitrate;
    _open = true;
    _status = BIF::CAN::Status::OK;
    return true;
}

void MockCan::close()
{
    _open = false;
    purge();
}

bool MockCan::isOpen()
{
    return _open;
}

bool MockCan::send(const BIF::CAN::Frame& frame)
{
    if (!_open || _peer == nullptr) {
        return false;
    }
    if (!_peer->pushRx(frame)) {
        _status = BIF::CAN::Status::OverFlowRX;
        return false;
    }
    return true;
}

bool MockCan::recv(BIF::CAN::Frame& out)
{
    if (!_open || _count == 0u) {
        return false;
    }
    out = _rx[_tail];
    _tail = (_tail + 1u) % kRxDepth;
    --_count;
    return true;
}

std::size_t MockCan::available() const
{
    return _count;
}

std::size_t MockCan::availableForWrite() const
{
    if (_peer == nullptr) {
        return 0u;
    }
    return kRxDepth - _peer->_count;
}

void MockCan::purge()
{
    _head = 0;
    _tail = 0;
    _count = 0;
}

void MockCan::purgeOutput()
{
    /* TX мгновенный — буфера нет. */
}

void MockCan::flush()
{
    /* TX мгновенный. */
}

BIF::CAN::Status MockCan::getStatus()
{
    return _status;
}

void MockCan::clearErrors()
{
    _status = BIF::CAN::Status::OK;
}

bool MockCan::pushRx(const BIF::CAN::Frame& frame) noexcept
{
    if (_count >= kRxDepth) {
        _status = BIF::CAN::Status::OverFlowRX;
        return false;
    }
    _rx[_head] = frame;
    _head = (_head + 1u) % kRxDepth;
    ++_count;
    return true;
}

} // namespace smcp
