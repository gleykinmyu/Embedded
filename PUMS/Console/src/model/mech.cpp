/**
 * @file mech.cpp
 * @brief Mech консоли: TX через MConsole::owner.
 */

#include "model/mech.hpp"
#include "model/mconsole.hpp"

MConsole* Mech::s_owner = nullptr;

Mech::Mech() noexcept : IMech(0), _type(Type::Rope)
{
    /* Ready до первой телеметрии — stub для UI. */
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
    if (s_owner == nullptr) {
        return false;
    }

    smcp::Selection sel;
    sel.add(_id);

    if (console_id == smcp::kSelectOwnerNone) {
        return s_owner->pushSelect(smcp::msg::Select::Action::Deselect, sel);
    }

    if (_status.any(Status::Blocked)) {
        return false;
    }

    return s_owner->pushSelect(smcp::msg::Select::Action::Select, sel);
}

bool Mech::block(bool blocked) noexcept
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

bool Mech::setTarget(const smcp::MotionTarget& target) noexcept
{
    (void)target;
    /* TODO: TX SetTarget через s_owner. */
    return false;
}

bool Mech::resetFault() noexcept
{
    /* TODO: TX ResetFault через s_owner. */
    return false;
}

void Mech::onTelemetry(uint8_t src_id, const smcp::msg::Telemetry& telemetry) noexcept
{
    if (!smcp::msg::isServerId(src_id) || telemetry.mech_id != _id) {
        return;
    }

    /* Show-Blocked на консоли локальный — сервер его не снимает. */
    const bool blocked = _status.any(Status::Blocked);

    _select_owner_id = telemetry.select_owner_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;

    if (blocked) {
        _status.set(Status::Blocked);
    }
}
