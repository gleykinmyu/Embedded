/**
 * @file ilink.cpp
 * @brief Общая SMCP-логика ILink (сборка Packet + isOpen).
 */

#include "smcp/transport/ilink.hpp"
#include "smcp/debug.hpp"

namespace smcp {

bool ILink::send(msg::Packet pkt) noexcept
{
    if (!isOpen()) {
        _status = Status::Closed;
        return false;
    }

    pkt.src_id = _node_id;

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
