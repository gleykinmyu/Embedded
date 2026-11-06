/**
 * @file group_bank.cpp
 * @brief CGMech: Select с проверкой overlap.
 */

#include "smcp/show/group_bank.hpp"

namespace smcp::show {

CGMech::CGMech(segment::Console& console, IGroupBank& groups) noexcept
    : Axis(console)
    , _groups(groups)
{}

CGroup::Result CGMech::trySelect(uint8_t console_id) noexcept
{
    if (console_id != kHolderNone) {
        if (isBlocked()) {
            return CGroup::Result::Blocked;
        }
        const axis::Route r = route();
        if (r.empty()) {
            return CGroup::Result::NotSent;
        }
        segment::Selection one;
        one.add(r.axis_id);
        if (_groups.fillBlockedOverlap(IGroupBank::kNoExcept, one)) {
            return CGroup::Result::OverlapsBlocked;
        }
        if (isSelected()) {
            return CGroup::Result::Occupied;
        }
    }

    if (!select(console_id)) {
        return CGroup::Result::NotSent;
    }
    return CGroup::Result::Ok;
}

} // namespace smcp::show
