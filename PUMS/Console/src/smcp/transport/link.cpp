/**
 * @file link.cpp
 * @brief Реализация smcp::Link (Gateway).
 */

#include "smcp/transport/link.hpp"

namespace smcp {

Link::Link(BIF::CAN::ICAN& can, uint8_t node_id) noexcept
    : _can(can)
    , _node_id(node_id)
{}

bool Link::send(const msg::Message& body, uint8_t dst_id, uint8_t pkt_id) noexcept
{
    if (!_can.isOpen()) {
        _status = Status::CanClosed;
        return false;
    }

    const msg::MsgId id = msg::helpers::msgIdOf(body);

    msg::Packet pkt{};
    pkt.hdr.prio = msg::helpers::defaultPrio(id);
    pkt.hdr.src_id = _node_id;
    pkt.hdr.dst_id = dst_id;
    pkt.hdr.msg_id = id;
    pkt.pkt_id = pkt_id;
    pkt.body = body;

    BIF::CAN::Frame frame;
    if (!msg::helpers::toCanFrame(pkt, frame)) {
        _status = Status::EncodeFailed;
        return false;
    }

    if (!_can.send(frame)) {
        _status = Status::CanSendFailed;
        return false;
    }

    _status = Status::OK;
    return true;
}

bool Link::receive(msg::Packet& out) noexcept
{
    if (!_can.isOpen()) {
        _status = Status::CanClosed;
        return false;
    }

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
