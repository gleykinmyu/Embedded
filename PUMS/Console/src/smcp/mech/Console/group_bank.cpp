/**
 * @file group_bank.cpp
 * @brief CGMech: Select с проверкой overlap.
 */

#include "smcp/mech/Console/group_bank.hpp"

namespace smcp {

CGMech::CGMech(IConsole& console, IGroupBank& groups, uint8_t id) noexcept
    : CGMech(console, groups, msg::kServerIdMin, id, id)
{}

CGMech::CGMech(IConsole& console, IGroupBank& groups, uint8_t server_id, uint8_t id) noexcept
    : CGMech(console, groups, server_id, id, id)
{}

CGMech::CGMech(IConsole& console, IGroupBank& groups, uint8_t server_id, uint8_t mech_id,
               uint8_t local_id) noexcept
    : CMech(console, server_id, mech_id, local_id)
    , _groups(groups)
{}

CGroup::Result CGMech::trySelect(uint8_t console_id) noexcept
{
    if (console_id != kHolderNone) {
        if (isBlocked()) {
            return CGroup::Result::Blocked;
        }
        Selection one;
        one.add(id());
        if (_groups.fillBlockedOverlap(IGroupBank::kNoExcept, one)) {
            return CGroup::Result::OverlapsBlocked;
        }
        if (isSelected()) {
            return CGroup::Result::Occupied;
        }
    }

    Selection one;
    one.add(id());
    const msg::Action action =
        (console_id == kHolderNone) ? msg::Action::Remove : msg::Action::Add;
    ServerSession* const s = console().server(serverId());
    if (s == nullptr || !s->select(action, one)) {
        return CGroup::Result::NotSent;
    }
    return CGroup::Result::Ok;
}

} // namespace smcp
