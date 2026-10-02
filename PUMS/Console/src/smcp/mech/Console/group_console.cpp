/**
 * @file group_console.cpp
 * @brief CGroup и билет queued group на Select Ack/Nack.
 */

#include "smcp/mech/Console/group_bank.hpp"

namespace smcp {

namespace {
const Selection kEmptySel{};
} // namespace

CGroup::CGroup(Group& rec, IGroupBank& bank, IGroupConsole& console) noexcept
    : _rec(&rec)
    , _bank(&bank)
    , _console(&console)
{}

uint8_t CGroup::id() const noexcept
{
    return _rec != nullptr ? _rec->id : 0u;
}

const Selection& CGroup::mech() const noexcept
{
    return _rec != nullptr ? _rec->mech : kEmptySel;
}

REG::BitMask<Group::Flag> CGroup::flags() const noexcept
{
    return _rec != nullptr ? _rec->flag : REG::BitMask<Group::Flag>{};
}

const char* CGroup::name() const noexcept
{
    return _rec != nullptr ? _rec->name : "";
}

bool CGroup::isEmpty() const noexcept
{
    return _rec == nullptr || _rec->isEmpty();
}

bool CGroup::contains(uint8_t mech_id) const noexcept
{
    return _rec != nullptr && _rec->mech.contains(mech_id);
}

bool CGroup::isBlocked() const noexcept
{
    return _rec != nullptr && _rec->isBlocked();
}

void CGroup::setBlocked(bool on) noexcept
{
    if (_rec == nullptr || _bank == nullptr || _console == nullptr) {
        return;
    }
    if (on) {
        (void)_bank->fillBlockedOverlap(_rec->id, _rec->mech);
    }
    if (_rec->isBlocked() == on) {
        return;
    }
    if (on) {
        if (_console->queuedGroup() == _rec->id || _console->activeGroup() == _rec->id) {
            _console->clearActiveGroup();
        }
    }
    _rec->setBlocked(on);
    _bank->markEdited();
}

CGroup::Result CGroup::recall() noexcept
{
    if (isEmpty()) {
        return Result::Empty;
    }
    if (isBlocked()) {
        return Result::Blocked;
    }
    if (_rec == nullptr || _bank == nullptr || _console == nullptr) {
        return Result::Empty;
    }
    if (_bank->fillBlockedOverlap(_rec->id, _rec->mech)) {
        return Result::OverlapsBlocked;
    }
    const uint8_t peer = _console->serverId(0u);
    ServerSession* const s = _console->server(peer);
    if (s == nullptr || !s->setSelection(_rec->mech)) {
        return Result::NotSent;
    }
    _console->setQueuedGroup(*this, *s);
    return Result::Ok;
}

CGroup::Result CGroup::record(Selection selection, const char* name, bool confirmed) noexcept
{
    if (_rec == nullptr || _bank == nullptr || selection.empty()) {
        return Result::Empty;
    }
    if (_bank->fillBlockedOverlap(_rec->id, selection)) {
        return Result::OverlapsBlocked;
    }
    if (!isEmpty() && !confirmed) {
        return Result::Occupied;
    }
    _rec->mech = selection;
    if (name != nullptr && name[0] != '\0') {
        _rec->setName(name);
    }
    _bank->markEdited();
    return Result::Ok;
}

bool CGroup::rename(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0' || isEmpty()) {
        return false;
    }
    if (_rec == nullptr || _bank == nullptr) {
        return false;
    }
    _rec->setName(name);
    _bank->markEdited();
    return true;
}

bool CGroup::clear(bool confirmed) noexcept
{
    if (isEmpty()) {
        return true;
    }
    if (!confirmed || _bank == nullptr || _rec == nullptr) {
        return false;
    }
    _rec->clear();
    _bank->markEdited();
    return true;
}

void IGroupConsole::onAck(Session* session, const TxSlot& req, const msg::Ack& reply) noexcept
{
    Node::onAck(session, req, reply);
    if (session == nullptr || !_queued) {
        return;
    }
    if (req.msg.id != msg::Select::kId || req.pkt_id != _queuedPkt) {
        return;
    }
    const CGroup group = _queued;
    const uint8_t id = group.id();
    clearQueuedGroup();
    _active = group;
    onGroupAck(id);
}

void IGroupConsole::onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept
{
    Node::onNack(session, req, reply);
    if (session != nullptr && _queued && req.msg.id == msg::Select::kId
        && req.pkt_id == _queuedPkt) {
        clearQueuedGroup();
    }
}

void IGroupConsole::onFault(Session* session, Fault reason) noexcept
{
    Node::onFault(session, reason);
    _active = CGroup{};
    clearQueuedGroup();
}

void IGroupConsole::onLink(Session* session, bool up) noexcept
{
    Node::onLink(session, up);
    if (!up && session != nullptr) {
        _active = CGroup{};
        clearQueuedGroup();
    }
}

void IGroupConsole::onStatus(Status status) noexcept
{
    Node::onStatus(status);
    if (status != Status::Ready && status != Status::LinkError) {
        _active = CGroup{};
        clearQueuedGroup();
    }
}

GServerSession::GServerSession(IGroupConsole& console) noexcept
    : ServerSession(console)
    , _group(console)
{}

bool GServerSession::select(msg::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return false;
    }
    _group.clearQueuedGroup();
    return ServerSession::select(action, selection);
}

void IGroupConsole::setQueuedGroup(CGroup group, ServerSession& session) noexcept
{
    _queued = group;
    _queuedPkt = session.pktId();
}

void IGroupConsole::clearActiveGroup() noexcept
{
    const CGroup groups[2] = { _active, _queued };
    _active = CGroup{};
    clearQueuedGroup();

    for (uint8_t i = 0u; i < 2u; ++i) {
        const CGroup& group = groups[i];
        if (!group || group.mech().empty()) {
            continue;
        }
        const uint8_t peer = serverId(0u);
        if (peer == 0u) {
            continue;
        }
        ServerSession* const link = server(peer);
        if (link == nullptr || !link->isOpen()) {
            continue;
        }
        link->select(msg::Action::Remove, group.mech());
    }
}

} // namespace smcp
