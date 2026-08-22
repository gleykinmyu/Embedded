/**
 * @file can_link.cpp
 * @brief Реализация smcp::CanLink (ICAN gateway).
 */

#include "smcp/transport/can_link.hpp"

namespace smcp {

CanLink::CanLink(BIF::CAN::ICAN& can, uint8_t node_id) noexcept
    : ILink(node_id)
    , _can(can)
{}

bool CanLink::isOpen() noexcept
{
    return _can.isOpen();
}

bool CanLink::write(const msg::Packet& pkt) noexcept
{
    BIF::CAN::Frame frame;
    if (!msg::helpers::toCanFrame(pkt, frame)) {
        _status = Status::EncodeFailed;
        return false;
    }

    if (!_can.send(frame)) {
        _status = Status::SendFailed;
        return false;
    }

    _status = Status::OK;
    return true;
}

bool CanLink::read(msg::Packet& out) noexcept
{
    BIF::CAN::Frame frame;
    if (!_can.recv(frame)) {
        return false;
    }

    if (!msg::helpers::fromCanFrame(frame, out)) {
        _status = Status::DecodeFailed;
        return false;
    }

    _status = Status::OK;
    return true;
}

} // namespace smcp
