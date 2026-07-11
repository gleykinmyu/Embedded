/**
 * @file mech.cpp
 * @brief Реализация smcp::IMech.
 */

#include "mech.hpp"
#include "message.hpp"

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

uint8_t IMech::owner_id() const noexcept
{
    return _owner_id;
}

bool IMech::isLeased() const noexcept
{
    return _owner_id != kLeaseOwnerNone;
}

bool IMech::isLeasedBy(uint8_t console_id) const noexcept
{
    return console_id != kLeaseOwnerNone && _owner_id == console_id;
}

void IMech::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    if (!msg::isServerId(src_id) || telemetry.local_id != _id) {
        return;
    }

    _owner_id = telemetry.owner_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;
}

} // namespace smcp
