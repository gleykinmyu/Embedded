/**
 * @file drive_mech.cpp
 */

#include "Server/model/drive_mech.hpp"

DriveMech::DriveMech(smcp::IServer& owner, uint8_t id, Type type) noexcept
    : IMech(id)
    , _type(type)
{
    smcp::detail::registerMech(owner, *this);
    _status.set(Status::Ready);
}

void DriveMech::select(uint8_t console_id) noexcept
{
    _holder = console_id;
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

void DriveMech::setTarget(const smcp::MotionTarget& target) noexcept
{
    (void)target;
    /* TODO: привод — старт Moving / лимиты → acceptSetTarget. */
}

void DriveMech::resetFault() noexcept {}
