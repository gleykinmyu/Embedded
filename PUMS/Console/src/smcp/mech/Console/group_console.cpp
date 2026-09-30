/**
 * @file group_console.cpp
 * @brief CGroup и билет queued group на Select Ack/Nack.
 */

#include "smcp/mech/Console/group_bank.hpp"

namespace smcp {

CGroup::CGroup(GroupHeader* head, Selection* sel, IGroupBank* bank, IGroupConsole* console) noexcept
    : _head(head)
    , _sel(sel)
    , _bank(bank)
    , _console(console)
{
    if (_head == nullptr || _sel == nullptr) {
        _head = nullptr;
        _sel = nullptr;
    }
}

CGroup::operator bool() const noexcept
{
    return _head != nullptr;
}

GroupHeader* CGroup::header() noexcept
{
    return _head;
}

const GroupHeader* CGroup::header() const noexcept
{
    return _head;
}

Selection* CGroup::at(uint8_t index) noexcept
{
    if (_sel == nullptr || _head == nullptr || index >= _head->count) {
        return nullptr;
    }
    return _sel + index;
}

const Selection* CGroup::at(uint8_t index) const noexcept
{
    if (_sel == nullptr || _head == nullptr || index >= _head->count) {
        return nullptr;
    }
    return _sel + index;
}

bool CGroup::isEmpty() const noexcept
{
    if (_head == nullptr) {
        return true;
    }
    for (uint8_t i = 0u; i < _head->count; ++i) {
        if (!_sel[i].empty()) {
            return false;
        }
    }
    return true;
}

bool CGroup::contains(uint8_t index, uint8_t mech_id) const noexcept
{
    const Selection* const sel = at(index);
    return sel != nullptr && sel->contains(mech_id);
}

Selection CGroup::merged() const noexcept
{
    Selection all;
    if (_head == nullptr) {
        return all;
    }
    for (uint8_t i = 0u; i < _head->count; ++i) {
        all = all | _sel[i];
    }
    return all;
}

bool CGroup::setAt(uint8_t index, Selection selection) noexcept
{
    Selection* const dst = at(index);
    if (dst == nullptr) {
        return false;
    }
    *dst = selection;
    return true;
}

uint8_t CGroup::id() const noexcept
{
    return _head != nullptr ? _head->id : 0u;
}

REG::BitMask<GroupHeader::Flag> CGroup::flags() const noexcept
{
    return _head != nullptr ? _head->flag : REG::BitMask<GroupHeader::Flag>{};
}

const char* CGroup::name() const noexcept
{
    return _head != nullptr ? _head->name : "";
}

bool CGroup::isBlocked() const noexcept
{
    return _head != nullptr && _head->isBlocked();
}

void CGroup::setBlocked(bool on) noexcept
{
    if (_head == nullptr || _bank == nullptr || _console == nullptr) {
        return;
    }
    if (on) {
        for (uint8_t seg = 0u; seg < _head->count; ++seg) {
            const Selection* const sel = at(seg);
            if (sel == nullptr || sel->empty()) {
                continue;
            }
            if (_bank->fillBlockedOverlap(_head->id, seg, *sel)) {
                break;
            }
        }
    }
    if (_head->isBlocked() == on) {
        return;
    }
    if (on) {
        if (_console->queuedGroup() == _head->id || _console->activeGroup() == _head->id) {
            _console->clearActiveGroup();
        }
    }
    _head->setBlocked(on);
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
    if (_head == nullptr || _bank == nullptr || _console == nullptr) {
        return Result::Empty;
    }
    for (uint8_t seg = 0u; seg < _head->count; ++seg) {
        const Selection* const sel = at(seg);
        if (sel == nullptr || sel->empty()) {
            continue;
        }
        if (_bank->fillBlockedOverlap(_head->id, seg, *sel)) {
            return Result::OverlapsBlocked;
        }
    }
    ServerSession* last = nullptr;
    const uint8_t n = _head->count;
    for (uint8_t i = 0u; i < n; ++i) {
        const Selection* const sel = at(i);
        if (sel == nullptr || sel->empty()) {
            continue;
        }
        const uint8_t peer = _console->serverId(i);
        ServerSession* const s = _console->server(peer);
        if (s == nullptr || !s->setSelection(*sel)) {
            return Result::NotSent;
        }
        last = s;
    }
    if (last == nullptr) {
        return Result::Empty;
    }
    _console->setQueuedGroup(*this, *last);
    return Result::Ok;
}

CGroup::Result CGroup::recordSegments(const Selection* selection, uint8_t count, const char* name,
                                      bool confirmed) noexcept
{
    if (selection == nullptr || _head == nullptr || _bank == nullptr || count > _head->count) {
        return Result::Empty;
    }
    bool any = false;
    for (uint8_t i = 0u; i < count; ++i) {
        if (!selection[i].empty()) {
            any = true;
            break;
        }
    }
    if (!any) {
        return Result::Empty;
    }
    for (uint8_t i = 0u; i < count; ++i) {
        if (selection[i].empty()) {
            continue;
        }
        if (_bank->fillBlockedOverlap(_head->id, i, selection[i])) {
            return Result::OverlapsBlocked;
        }
    }
    if (!isEmpty() && !confirmed) {
        return Result::Occupied;
    }
    for (uint8_t i = 0u; i < count; ++i) {
        if (!setAt(i, selection[i])) {
            return Result::Empty;
        }
    }
    if (name != nullptr && name[0] != '\0') {
        _head->setName(name);
    }
    _bank->markEdited();
    return Result::Ok;
}

bool CGroup::rename(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0' || isEmpty()) {
        return false;
    }
    if (_head == nullptr || _bank == nullptr) {
        return false;
    }
    _head->setName(name);
    _bank->markEdited();
    return true;
}

bool CGroup::clear(bool confirmed) noexcept
{
    if (isEmpty()) {
        return true;
    }
    if (!confirmed || _bank == nullptr || _head == nullptr) {
        return false;
    }
    if (_sel != nullptr) {
        for (uint8_t i = 0u; i < _head->count; ++i) {
            _sel[i] = Selection{};
        }
    }
    _head->clear();
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
    const uint8_t id = group.header()->id;
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
        if (!group) {
            continue;
        }
        const uint8_t n = group.header()->count;
        for (uint8_t seg = 0u; seg < n; ++seg) {
            const Selection* const sel = group.at(seg);
            if (sel == nullptr || sel->empty()) {
                continue;
            }
            const uint8_t peer = serverId(seg);
            if (peer == 0u) {
                continue;
            }
            ServerSession* const link = server(peer);
            if (link == nullptr || !link->isOpen()) {
                continue;
            }
            link->select(msg::Action::Remove, *sel);
        }
    }
}

} // namespace smcp
