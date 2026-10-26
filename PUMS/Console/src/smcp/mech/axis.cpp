/**
 * @file axis.cpp
 * @brief Реализация smcp::IAxis.
 */

#include "smcp/mech/axis.hpp"

namespace smcp {

IAxis::IAxis(uint8_t id) noexcept
    : _id(id)
{}

IAxis::~IAxis() = default;

uint8_t IAxis::id() const noexcept
{
    return _id;
}

int32_t IAxis::position() const noexcept
{
    return _position;
}

REG::BitMask<IAxis::Status> IAxis::status() const noexcept
{
    return _status;
}

bool IAxis::isIdle() const noexcept
{
    return !_status.any(Status::Ready | Status::Moving);
}

uint8_t IAxis::holder() const noexcept
{
    return _holder;
}

bool IAxis::isSelected() const noexcept
{
    return _holder != kHolderNone;
}

bool IAxis::isSelectedBy(uint8_t console_id) const noexcept
{
    return console_id != kHolderNone && _holder == console_id;
}

bool IAxis::isBlocked() const noexcept
{
    return _status.any(Status::Blocked);
}

} // namespace smcp
