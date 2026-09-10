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

void DriveMech::select(uint8_t console_id) noexcept
{
    if (console_id == smcp::kHolderNone) {
        if (isSelected()) {
            _holder = smcp::kHolderNone;
            if (s_selectedCount > 0u) {
                --s_selectedCount;
            }
        }
        return;
    }

    if (isSelected()) {
        return; /* уже наш или чужой — чужой не трогаем (политика снаружи) */
    }

    if (_status.any(Status::Blocked) || !_status.any(Status::Ready)) {
        return;
    }

    if (s_selectedCount >= kMaxSelected) {
        return;
    }

    _holder = console_id;
    ++s_selectedCount;
}

void DriveMech::block(bool blocked) noexcept
{
    if (blocked) {
        if (isSelected()) {
            select(smcp::kHolderNone);
        }
        _status.set(Status::Blocked);
    } else {
        _status.clear(Status::Blocked);
    }
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
