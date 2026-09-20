/**
 * @file group_bank.cpp
 * @brief CGroup / CGMech: слот GRUP и Select с проверкой overlap.
 */

#include "smcp/GroupConsole/group_bank.hpp"

namespace smcp {

CGroup::CGroup(Group& rec, IGroupBank& bank, IGroupConsole& console) noexcept
    : _rec(rec)
    , _bank(bank)
    , _console(console)
{}

uint8_t CGroup::id() const noexcept
{
    return _rec.id;
}

const Selection& CGroup::mech() const noexcept
{
    return _rec.mech;
}

REG::BitMask<Group::Flag> CGroup::flags() const noexcept
{
    return _rec.flag;
}

const char* CGroup::name() const noexcept
{
    return _rec.name;
}

bool CGroup::isEmpty() const noexcept
{
    return _rec.isEmpty();
}

bool CGroup::isBlocked() const noexcept
{
    return _rec.isBlocked();
}

void CGroup::setBlocked(bool on) noexcept
{
    if (on) {
        (void)_bank.fillBlockedOverlap(_rec.id, _rec.mech);
    }
    if (_rec.isBlocked() == on) {
        return;
    }
    if (on) {
        if (_console.queuedGroup() == _rec.id || _console.activeGroup() == _rec.id) {
            _console.clearActiveGroup();
        }
    }
    _rec.setBlocked(on);
    _bank.markEdited();
}

CGroup::Result CGroup::recall() noexcept
{
    if (isEmpty()) {
        return Result::Empty;
    }
    if (isBlocked()) {
        return Result::Blocked;
    }
    if (_bank.fillBlockedOverlap(_rec.id, _rec.mech)) {
        return Result::OverlapsBlocked;
    }
    _console.setSelection(_rec.mech);
    _console.setQueuedGroup(_rec.id);
    return Result::Ok;
}

CGroup::Result CGroup::record(Selection selection, const char* name, bool confirmed) noexcept
{
    if (selection.empty()) {
        return Result::Empty;
    }

    if (_bank.fillBlockedOverlap(_rec.id, selection)) {
        return Result::OverlapsBlocked;
    }

    if (!isEmpty() && !confirmed) {
        return Result::Occupied;
    }

    _rec.mech = selection;
    if (name != nullptr && name[0] != '\0') {
        _rec.setName(name);
    }
    _bank.markEdited();
    return Result::Ok;
}

bool CGroup::rename(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0' || isEmpty()) {
        return false;
    }
    _rec.setName(name);
    _bank.markEdited();
    return true;
}

bool CGroup::clear(bool confirmed) noexcept
{
    if (isEmpty()) {
        return true;
    }
    if (!confirmed) {
        return false;
    }
    _rec.clear();
    _bank.markEdited();
    return true;
}

void CGroup::readFrom(const Group& rec) noexcept
{
    /* Id слота задаёт банк, не содержимое файла. */
    const uint8_t slot = _rec.id;
    _rec = rec;
    _rec.id = slot;
    _rec.name[kGroupNameSize - 1u] = '\0';
}

void CGroup::writeTo(Group& rec) const noexcept
{
    rec = _rec;
}

CGMech::CGMech(IConsole& console, IGroupBank& groups, uint8_t id) noexcept
    : CMech(console, id)
    , _groups(groups)
{}

CGroup::Result CGMech::trySelect(uint8_t console_id) noexcept
{
    if (console_id == kHolderNone) {
        CMech::select(console_id);
        return CGroup::Result::Ok;
    }
    if (isBlocked()) {
        return CGroup::Result::Blocked;
    }
    Selection one;
    one.add(_id);
    if (_groups.fillBlockedOverlap(IGroupBank::kNoExcept, one)) {
        return CGroup::Result::OverlapsBlocked;
    }
    if (isSelected()) {
        return CGroup::Result::Occupied;
    }
    CMech::select(console_id);
    return CGroup::Result::Ok;
}

} // namespace smcp
