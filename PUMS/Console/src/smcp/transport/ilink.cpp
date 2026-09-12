/**
 * @file ilink.cpp
 * @brief Общая SMCP-логика ILink (сборка Packet + isOpen).
 */

#include "smcp/transport/ilink.hpp"
#include "smcp/debug.hpp"

namespace smcp {

bool ILink::send(const msg::Message& body, uint8_t dst_id, uint8_t pkt_id) noexcept
{
    if (!isOpen()) {
        _status = Status::Closed;
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

    SMCP_TRACE_PKT("TX", _node_id, pkt);
    return write(pkt);
}

bool ILink::receive(msg::Packet& out) noexcept
{
    if (!isOpen()) {
        _status = Status::Closed;
        return false;
    }

    if (!read(out)) {
        return false;
    }
    SMCP_TRACE_PKT("RX", _node_id, out);
    return true;
}

} // namespace smcp
