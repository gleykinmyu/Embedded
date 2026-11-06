/**
 * @file group_console.cpp
 * @brief CGroup и билет queued group на Select Ack/Nack.
 */

#include "smcp/show/group_bank.hpp"

namespace smcp::show {

namespace {
const segment::Selection kEmptySel{};
} // namespace

CGroup::CGroup(Group& rec, IGroupBank& bank, GroupConsole& console) noexcept
    : _rec(&rec)
    , _bank(&bank)
    , _console(&console)
{}

uint8_t CGroup::id() const noexcept
{
    return _rec != nullptr ? _rec->id : 0u;
}

const segment::Selection& CGroup::sel() const noexcept
{
    return _rec != nullptr ? _rec->sel : kEmptySel;
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

bool CGroup::contains(uint8_t axis_id) const noexcept
{
    return _rec != nullptr && _rec->sel.contains(axis_id);
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
        (void)_bank->fillBlockedOverlap(_rec->id, _rec->sel);
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
    if (_bank->fillBlockedOverlap(_rec->id, _rec->sel)) {
        return Result::OverlapsBlocked;
    }
    Session* const s = _console->session(segment::kServerIdMin);
    if (s == nullptr || !s->setSelection(_rec->sel)) {
        return Result::NotSent;
    }
    _console->setQueuedGroup(*this, *s);
    return Result::Ok;
}

CGroup::Result CGroup::record(segment::Selection selection, const char* name, bool confirmed) noexcept
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
    _rec->sel = selection;
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

void GroupConsole::onAck(::smcp::Session* session, const TxSlot& req, const session::Ack& reply) noexcept
{
    Node::onAck(session, req, reply);
    if (session == nullptr || !_queued) {
        return;
    }
    if (req.msg.id != segment::Select::kId || req.txn_id != _queuedTxnId) {
        return;
    }
    const CGroup group = _queued;
    const uint8_t id = group.id();
    clearQueuedGroup();
    _active = group;
    onGroupAck(id);
}

void GroupConsole::onNack(::smcp::Session* session, const TxSlot& req, const session::Nack& reply) noexcept
{
    Node::onNack(session, req, reply);
    if (session != nullptr && _queued && req.msg.id == segment::Select::kId
        && req.txn_id == _queuedTxnId) {
        clearQueuedGroup();
    }
}

void GroupConsole::onLink(::smcp::Session* session, Link link) noexcept
{
    Console::onLink(session, link);
    if (link != Link::Up && session != nullptr) {
        _active = CGroup{};
        clearQueuedGroup();
    }
}

void GroupConsole::onState(State state) noexcept
{
    Node::onState(state);
    if (state != State::Ready) {
        _active = CGroup{};
        clearQueuedGroup();
    }
}

void GroupConsole::setQueuedGroup(CGroup group, Session& session) noexcept
{
    _queued = group;
    _queuedTxnId = session.txnId();
}

void GroupConsole::clearActiveGroup() noexcept
{
    const CGroup groups[2] = { _active, _queued };
    _active = CGroup{};
    clearQueuedGroup();

    for (uint8_t i = 0u; i < 2u; ++i) {
        const CGroup& group = groups[i];
        if (!group || group.sel().empty()) {
            continue;
        }
        Session* const link = session(segment::kServerIdMin);
        if (link == nullptr || !link->isOpen()) {
            continue;
        }
        link->select(segment::Action::Remove, group.sel());
    }
}

} // namespace smcp::show
