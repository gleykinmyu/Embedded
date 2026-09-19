/**
 * @file group_console.cpp
 * @brief IGroupConsole: билет queued group на Select Ack/Nack.
 */

#include "smcp/GroupConsole/group_console.hpp"

namespace smcp {

void IGroupConsole::onAck(Session* session, const TxSlot& req) noexcept
{
    Node::onAck(session, req);
    if (session != &_primary) {
        return;
    }
    if (msg::helpers::msgIdOf(req.body) != msg::MsgId::Select) {
        return;
    }
    if (_queuedGroup == kNoQueuedGroup) {
        return;
    }
    const uint8_t id = _queuedGroup;
    _queuedGroup = kNoQueuedGroup;
    onGroupAck(id);
}

void IGroupConsole::onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept
{
    Node::onNack(session, req, reply);
    if (session == &_primary && msg::helpers::msgIdOf(req.body) == msg::MsgId::Select) {
        _queuedGroup = kNoQueuedGroup;
    }
}

void IGroupConsole::setQueuedGroup(uint8_t group_id) noexcept
{
    _queuedGroup = group_id;
}

} // namespace smcp
