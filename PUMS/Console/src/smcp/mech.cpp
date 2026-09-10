/**
 * @file mech.cpp
 * @brief Реализация smcp::IMech.
 */

#include "mech.hpp"

namespace smcp {

IMech::IMech(uint8_t id) noexcept : _id(id) {}

IMech::~IMech() = default;

uint8_t IMech::id() const noexcept
{
    return _id;
}

int32_t IMech::position() const noexcept
{
    return _position;
}

REG::BitMask<IMech::Status> IMech::status() const noexcept
{
    return _status;
}

bool IMech::isIdle() const noexcept
{
    return !_status.any(Status::Ready | Status::Moving);
}

uint8_t IMech::holder() const noexcept
{
    return _holder;
}

bool IMech::isSelected() const noexcept
{
    return _holder != kHolderNone;
}

bool IMech::isSelectedBy(uint8_t console_id) const noexcept
{
    return console_id != kHolderNone && _holder == console_id;
}

bool IMech::isBlocked() const noexcept
{
    return _status.any(Status::Blocked);
}

} // namespace smcp
