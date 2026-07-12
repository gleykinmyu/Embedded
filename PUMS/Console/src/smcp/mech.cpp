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

void IMech::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    if (!msg::isServerId(src_id) || telemetry.local_id != _id) {
        return;
    }

    _select_owner_id = telemetry.select_owner_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;
}

Mech::Mech() noexcept : IMech(0), _type(Type::Rope)
{
    _status.set(Status::Ready);
}

Mech::Mech(uint8_t id, Type type) noexcept : IMech(id), _type(type)
{
    _status.set(Status::Ready);
}

Mech::Type Mech::type() const noexcept
{
    return _type;
}

bool Mech::select(uint8_t console_id) noexcept
{
    if (console_id == kSelectOwnerNone) {
        _select_owner_id = kSelectOwnerNone;
        _status.clear(Status::Selected);
        return true;
    }

    if (isSelected()) {
        return isSelectedBy(console_id);
    }

    if (!_status.any(Status::Ready)) {
        return false;
    }

    _select_owner_id = console_id;
    _status.set(Status::Selected);
    return true;
}

bool Mech::setTarget(const MotionTarget& target) noexcept
{
    (void)target;
    return false;
}

bool Mech::resetFault() noexcept
{
    return false;
}

} // namespace smcp
