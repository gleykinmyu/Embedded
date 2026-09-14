/**
 * @file cmech.cpp
 */

#include "smcp/Console/cmech.hpp"
#include "smcp/Console/console.hpp"

namespace smcp {

CMech::CMech(IConsole& console, uint8_t id) noexcept
    : IMech(id)
    , _console(&console)
{
    detail::registerMech(console, *this);
}

void CMech::select(uint8_t console_id) noexcept
{
    Selection sel;
    sel.add(_id);

    if (console_id == kHolderNone) {
        _console->select(msg::Action::Remove, sel);
        return;
    }

    _console->select(msg::Action::Add, sel);
}

void CMech::block(bool blocked) noexcept
{
    Selection sel;
    sel.add(_id);
    _console->block(blocked ? msg::Action::Add : msg::Action::Remove, sel);
}

void CMech::setTarget(const MotionTarget& target) noexcept
{
    _console->setTarget(_id, target);
}

void CMech::resetFault() noexcept {}

void CMech::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    if (!msg::helpers::isServerId(src_id) || telemetry.mech_id != _id) {
        return;
    }

    /* Зеркало сегмента: holder/status/position только с сервера. */
    _holder = telemetry.holder_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;
}

} // namespace smcp
