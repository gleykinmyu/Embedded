/**
 * @file drive_mech.cpp
 * @brief DriveMech: локальный select/deselect для MServer.
 */

#include "model/drive_mech.hpp"

uint8_t DriveMech::s_selectedCount = 0u;

DriveMech::DriveMech() noexcept : IMech(0), _type(Type::Rope)
{
    _status.set(Status::Ready);
}

DriveMech::DriveMech(uint8_t id, Type type) noexcept : IMech(id), _type(type)
{
    _status.set(Status::Ready);
}

bool DriveMech::select(uint8_t console_id) noexcept
{
    if (console_id == smcp::kSelectOwnerNone) {
        if (isSelected()) {
            _select_owner_id = smcp::kSelectOwnerNone;
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

    _select_owner_id = console_id;
    _status.set(Status::Selected);
    ++s_selectedCount;
    return true;
}

bool DriveMech::block(bool blocked) noexcept
{
    if (blocked) {
        if (isSelected()) {
            (void)select(smcp::kSelectOwnerNone);
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
