/**
 * @file mech.cpp
 */

#include "model/mech.hpp"

Mech::Mech(smcp::IConsole& console, uint8_t id, Type type) noexcept
    : IMech(id)
    , _console(&console)
    , _type(type)
{
    smcp::detail::registerMech(console, *this);
}

Mech::Type Mech::type() const noexcept
{
    return _type;
}

bool Mech::select(uint8_t console_id) noexcept
{
    smcp::Selection sel;
    sel.add(_id);

    if (console_id == smcp::kHolderNone) {
        _console->select(smcp::msg::Select::Action::Deselect, sel);
        return true;
    }

    if (_status.any(Status::Blocked)) {
        return false;
    }

    _console->select(smcp::msg::Select::Action::Select, sel);
    return true;
}

bool Mech::block(bool blocked) noexcept
{
    if (blocked) {
        if (isSelected()) {
            (void)select(smcp::kHolderNone);
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
    return false;
}

bool Mech::resetFault() noexcept
{
    return false;
}

void Mech::onTelemetry(uint8_t src_id, const smcp::msg::Telemetry& telemetry) noexcept
{
    if (!smcp::msg::helpers::isServerId(src_id) || telemetry.mech_id != _id) {
        return;
    }

    const bool blocked = _status.any(Status::Blocked);

    _holder = telemetry.holder_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;

    if (blocked) {
        _status.set(Status::Blocked);
    }
}
