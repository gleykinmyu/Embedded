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

uint8_t IMech::select_owner_id() const noexcept
{
    return _select_owner_id;
}

bool IMech::isSelected() const noexcept
{
    return _select_owner_id != kSelectOwnerNone;
}

bool IMech::isSelectedBy(uint8_t console_id) const noexcept
{
    return console_id != kSelectOwnerNone && _select_owner_id == console_id;
}

bool IMech::isBlocked() const noexcept
{
    return _status.any(Status::Blocked);
}

void IMech::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    if (!msg::isServerId(src_id) || telemetry.mech_id != _id) {
        return;
    }

    _select_owner_id = telemetry.select_owner_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;
}

} // namespace smcp
