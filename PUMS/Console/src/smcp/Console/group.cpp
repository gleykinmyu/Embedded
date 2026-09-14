/**
 * @file group.cpp
 * @brief CGroup: recall / record / rename / clear / readFrom (прокси Group + IConsole).
 */

#include "smcp/Console/group.hpp"
#include "smcp/Console/console.hpp"

namespace smcp {

bool CGroup::recall() noexcept
{
    if (isEmpty()) {
        return false;
    }
    _console.setSelection(_rec.mech);
    return true;
}

bool CGroup::record(Selection selection, const char* name, bool confirmed) noexcept
{
    if (selection.empty()) {
        return false;
    }

    if (!isEmpty() && !confirmed) {
        return false;
    }

    _rec.mech = selection;
    if (name != nullptr && name[0] != '\0') {
        _rec.setName(name);
    }
    _section.markEdited();
    return true;
}

bool CGroup::rename(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0' || isEmpty()) {
        return false;
    }
    _rec.setName(name);
    _section.markEdited();
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
    _section.markEdited();
    return true;
}

void CGroup::setBlocked(bool on) noexcept
{
    if (_rec.isBlocked() == on) {
        return;
    }
    _rec.setBlocked(on);
    _section.markEdited();
}

void CGroup::readFrom(const Group& rec) noexcept
{
    const uint8_t slot = _rec.id;
    _rec = rec;
    _rec.id = slot;
    _rec.name[kGroupNameSize - 1u] = '\0';
}

} // namespace smcp
