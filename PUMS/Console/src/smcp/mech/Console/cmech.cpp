/**
 * @file cmech.cpp
 * @brief CMech: Select/Block/SetTarget через IConsole, зеркало Telemetry.
 */

#include "smcp/mech/Console/cmech.hpp"
#include "smcp/mech/Console/console.hpp"

namespace smcp {

CMech::CMech(IConsole& console, uint8_t id) noexcept
    : CMech(console, kPrimaryServer, id)
{}

CMech::CMech(IConsole& console, uint8_t server_id, uint8_t id) noexcept
    : IMech(id)
    , _console(&console)
    , _server_id(server_id)
{
    detail::registerMech(console, *this);
}

void CMech::select(uint8_t console_id) noexcept
{
    Selection sel;
    sel.add(_id);
    _console->select(console_id == kHolderNone ? msg::Action::Remove : msg::Action::Add, sel,
                     _server_id);
}

void CMech::block(bool blocked) noexcept
{
    Selection sel;
    sel.add(_id);
    _console->block(blocked ? msg::Action::Add : msg::Action::Remove, sel, _server_id);
}

void CMech::setTarget(const MotionTarget& target) noexcept
{
    _console->setTarget(_id, target, _server_id);
}

void CMech::resetFault() noexcept
{
    /* Fault сбрасывает сервер; на пульте локального состояния нет. */
}

void CMech::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    if (telemetry.mech_id != _id) {
        return;
    }
    if (_server_id != kPrimaryServer && src_id != _server_id) {
        return;
    }

    /* Зеркало сегмента: holder/status/position только с сервера. */
    _holder = telemetry.holder_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;
}

} // namespace smcp
