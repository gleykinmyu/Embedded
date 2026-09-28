/**
 * @file group_console.cpp
 * @brief IGroupConsole: билет queued group на Select Ack/Nack; HB lost чистит active.
 */

#include "smcp/mech/Console/group_console.hpp"

namespace smcp {

void IGroupConsole::onAck(Session* session, const TxSlot& req, const msg::Ack& reply) noexcept
{
    Node::onAck(session, req, reply);
    if (session != &_primary) {
        return;
    }
    if (req.msg.id != msg::Select::kId) {
        return;
    }
    if (_queuedGroup == kNoQueuedGroup) {
        return;
    }
    const uint8_t id = _queuedGroup;
    _queuedGroup = kNoQueuedGroup;
    _activeGroup = id;
    onGroupAck(id);
}

void IGroupConsole::onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept
{
    Node::onNack(session, req, reply);
    if (session == &_primary && req.msg.id == msg::Select::kId) {
        _queuedGroup = kNoQueuedGroup;
    }
}

void IGroupConsole::onFault(Session* session, Fault reason) noexcept
{
    Node::onFault(session, reason);
    if (session == nullptr || session == &_primary) {
        _activeGroup = kNoActiveGroup;
        _queuedGroup = kNoQueuedGroup;
    }
}

void IGroupConsole::onLink(Session* session, bool up) noexcept
{
    Node::onLink(session, up);
    if (!up && session == &_primary) {
        _activeGroup = kNoActiveGroup;
        _queuedGroup = kNoQueuedGroup;
    }
}

void IGroupConsole::onStatus(Status status) noexcept
{
    Node::onStatus(status);
    if (status != Status::Ready && status != Status::LinkError) {
        _activeGroup = kNoActiveGroup;
        _queuedGroup = kNoQueuedGroup;
    }
}

void IGroupConsole::setQueuedGroup(uint8_t group_id) noexcept
{
    _queuedGroup = group_id;
}

void IGroupConsole::clearActiveGroup() noexcept
{
    _activeGroup = kNoActiveGroup;
    _queuedGroup = kNoQueuedGroup;
    clearSelection();
}

} // namespace smcp
