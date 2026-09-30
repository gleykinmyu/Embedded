/**
 * @file group_bank.cpp
 * @brief CGMech: Select с проверкой overlap.
 */

#include "smcp/mech/Console/group_bank.hpp"

namespace smcp {

CGMech::CGMech(IConsole& console, IGroupBank& groups, uint8_t id) noexcept
    : CGMech(console, groups, msg::kServerIdMin, id)
{}

CGMech::CGMech(IConsole& console, IGroupBank& groups, uint8_t server_id, uint8_t id) noexcept
    : CMech(console, server_id, id)
    , _groups(groups)
{}

CGroup::Result CGMech::trySelect(uint8_t console_id) noexcept
{
    if (console_id != kHolderNone) {
        if (isBlocked()) {
            return CGroup::Result::Blocked;
        }
        Selection one;
        one.add(_id);
        const uint8_t index = console().serverIndex(serverId());
        if (_groups.fillBlockedOverlap(IGroupBank::kNoExcept, index, one)) {
            return CGroup::Result::OverlapsBlocked;
        }
        if (isSelected()) {
            return CGroup::Result::Occupied;
        }
    }

    Selection one;
    one.add(_id);
    const msg::Action action = (console_id == kHolderNone)
        ? msg::Action::Remove
        : msg::Action::Add;
    ServerSession* const s = console().server(serverId());
    if (s == nullptr || !s->select(action, one)) {
        return CGroup::Result::NotSent;
    }
    return CGroup::Result::Ok;
}

} // namespace smcp
