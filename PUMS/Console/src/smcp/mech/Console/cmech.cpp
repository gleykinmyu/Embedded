/**
 * @file cmech.cpp
 * @brief CMech: Select/Block/SetTarget через IConsole, зеркало Telemetry.
 */

#include "smcp/mech/Console/cmech.hpp"
#include "smcp/mech/Console/console.hpp"

namespace smcp {

CMech::CMech(IConsole& console, uint8_t id) noexcept
    : CMech(console, msg::kServerIdMin, id, id)
{}

CMech::CMech(IConsole& console, uint8_t server_id, uint8_t id) noexcept
    : CMech(console, server_id, id, id)
{}

CMech::CMech(IConsole& console, uint8_t server_id, uint8_t mech_id, uint8_t local_id) noexcept
    : IMech(mech_id)
    , _console(&console)
    , _server_id(server_id)
    , _local_id(local_id)
{
    detail::registerMech(console, *this);
}

void CMech::select(uint8_t console_id) noexcept
{
    Selection sel;
    sel.add(_id);
    ServerSession* const s = _console->server(_server_id);
    if (s == nullptr) {
        return;
    }
    s->select(console_id == kHolderNone ? msg::Action::Remove : msg::Action::Add, sel);
}

void CMech::block(bool blocked) noexcept
{
    Selection sel;
    sel.add(_id);
    ServerSession* const s = _console->server(_server_id);
    if (s == nullptr) {
        return;
    }
    s->block(blocked ? msg::Action::Add : msg::Action::Remove, sel);
}

void CMech::setTarget(const MotionTarget& target) noexcept
{
    ServerSession* const s = _console->server(_server_id);
    if (s == nullptr) {
        return;
    }
    s->setTarget(_id, target);
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
    if (src_id != _server_id) {
        return;
    }

    /* Зеркало сегмента: holder/status/position только с сервера. */
    _holder = telemetry.holder_id;
    _position = telemetry.position_mm;
    _status = telemetry.status;
}

} // namespace smcp
