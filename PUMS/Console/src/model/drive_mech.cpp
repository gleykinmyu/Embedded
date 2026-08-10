/**
 * @file drive_mech.cpp
 */

#include "model/drive_mech.hpp"

uint8_t DriveMech::s_selectedCount = 0u;

DriveMech::DriveMech(smcp::IServer& owner, uint8_t id, Type type) noexcept
    : IMech(id)
    , _type(type)
{
    smcp::detail::registerMech(owner, *this);
    _status.set(Status::Ready);
}

bool DriveMech::select(uint8_t console_id) noexcept
{
    if (console_id == smcp::kHolderNone) {
        if (isSelected()) {
            _holder = smcp::kHolderNone;
            _status.clear(Status::Selected);
            if (s_selectedCount > 0u) {
                --s_selectedCount;
            }
        }
        return true;
    }

    if (isSelected()) {
        return isSelectedBy(console_id);
    }

    if (_status.any(Status::Blocked) || !_status.any(Status::Ready)) {
        return false;
    }

    if (s_selectedCount >= kMaxSelected) {
        return false;
    }

    _holder = console_id;
    _status.set(Status::Selected);
    ++s_selectedCount;
    return true;
}

bool DriveMech::block(bool blocked) noexcept
{
    if (blocked) {
        if (isSelected()) {
            (void)select(smcp::kHolderNone);
        }
        _status.set(Status::Blocked);
    } else {
        _status.clear(Status::Blocked);
    }
    return true;
}

bool DriveMech::setTarget(const smcp::MotionTarget& target) noexcept
{
    (void)target;
    return false;
}

bool DriveMech::resetFault() noexcept
{
    return false;
}
