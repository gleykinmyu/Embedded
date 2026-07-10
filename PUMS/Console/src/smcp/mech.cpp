/**
 * @file mech.cpp
 * @brief Реализация smcp::IMech.
 */

#include "mech.hpp"

namespace smcp {

IMech::IMech(uint8_t id) noexcept : id_(id) {}

IMech::~IMech() = default;

uint8_t IMech::id() const noexcept
{
    return id_;
}

bool IMech::isIdle() const noexcept
{
    const auto mask = status();
    return !mask.any(Status::Ready | Status::Moving);
}

uint8_t IMech::leaseOwner() const noexcept
{
    return lease_owner_;
}

bool IMech::isLeased() const noexcept
{
    return lease_owner_ != kLeaseOwnerNone;
}

bool IMech::isLeasedBy(uint8_t console_id) const noexcept
{
    return console_id != kLeaseOwnerNone && lease_owner_ == console_id;
}

void IMech::setLeaseOwner(uint8_t console_id) noexcept
{
    lease_owner_ = console_id;
}

} // namespace smcp
