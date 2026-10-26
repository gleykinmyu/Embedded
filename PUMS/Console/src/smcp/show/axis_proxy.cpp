/**
 * @file axis_proxy.cpp
 * @brief AxisProxy / AxisProxyTable: TX через IConsole, зеркало Telemetry.
 */

#include "smcp/show/axis_proxy.hpp"

#include "smcp/mech/Console/console.hpp"

namespace smcp {
namespace show {

AxisProxy::AxisProxy() noexcept
    : IAxis(0)
{}

void AxisProxy::bind(IConsole& console, AxisPool<>& show_patch, uint8_t slot_index) noexcept
{
    _console = &console;
    _show = &show_patch;
    _slot = slot_index;
    syncFromPatch();
}

void AxisProxy::syncFromPatch() noexcept
{
    Patch* const p = patch();
    if (p == nullptr || !p->isPatched()) {
        setId(0);
        _type = nullptr;
        return;
    }
    setId(p->route.axis_id);
    _type = _show != nullptr ? _show->typeOf(*p) : nullptr;
}

bool AxisProxy::isBound() const noexcept
{
    return _console != nullptr && _show != nullptr;
}

bool AxisProxy::isEmpty() const noexcept
{
    const Patch* p = patch();
    return p == nullptr || p->isEmpty();
}

bool AxisProxy::isPatched() const noexcept
{
    const Patch* p = patch();
    return p != nullptr && p->isPatched();
}

Patch* AxisProxy::patch() noexcept
{
    return _show != nullptr ? _show->patch.get(_slot) : nullptr;
}

const Patch* AxisProxy::patch() const noexcept
{
    return _show != nullptr ? _show->patch.get(_slot) : nullptr;
}

axis::Route AxisProxy::route() const noexcept
{
    const Patch* p = patch();
    return p != nullptr ? p->route : axis::Route{};
}

uint16_t AxisProxy::showId() const noexcept
{
    const Patch* p = patch();
    return p != nullptr ? p->id : kIdNone;
}

const char* AxisProxy::name() const noexcept
{
    const Patch* p = patch();
    return p != nullptr ? p->name : "";
}

void AxisProxy::select(uint8_t console_id) noexcept
{
    if (!isPatched() || _console == nullptr) {
        return;
    }
    const axis::Route r = route();
    Selection sel;
    sel.add(r.axis_id);
    ServerSession* const s = _console->server(r.server_id);
    if (s == nullptr) {
        return;
    }
    s->select(console_id == kHolderNone ? msg::Action::Remove : msg::Action::Add, sel);
}

void AxisProxy::block(bool blocked) noexcept
{
    if (!isPatched() || _console == nullptr) {
        return;
    }
    const axis::Route r = route();
    Selection sel;
    sel.add(r.axis_id);
    ServerSession* const s = _console->server(r.server_id);
    if (s == nullptr) {
        return;
    }
    s->block(blocked ? msg::Action::Add : msg::Action::Remove, sel);
}

void AxisProxy::setTarget(const MotionTarget& target) noexcept
{
    if (!isPatched() || _console == nullptr) {
        return;
    }
    const axis::Route r = route();
    ServerSession* const s = _console->server(r.server_id);
    if (s == nullptr) {
        return;
    }
    s->setTarget(r.axis_id, target);
}

void AxisProxy::resetFault() noexcept
{
    if (!isPatched() || _console == nullptr) {
        return;
    }
    const axis::Route r = route();
    ServerSession* const s = _console->server(r.server_id);
    if (s == nullptr) {
        return;
    }
    s->resetFault(Selection::of(r.axis_id));
}

void AxisProxy::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    if (!isPatched()) {
        return;
    }
    const axis::Route r = route();
    if (src_id != r.server_id || telemetry.axis_id != r.axis_id) {
        return;
    }
    _holder = telemetry.holder_id;
    _position = telemetry.position;
    _status = telemetry.status;
}

void AxisProxy::clearLive() noexcept
{
    _holder = kHolderNone;
    _position = 0;
    _status = {};
    _profile = nullptr;
}

AxisProxyTable::AxisProxyTable(AxisPool<>& show_patch, IConsole& console) noexcept
    : _show(&show_patch)
    , _console(&console)
{
    for (uint8_t i = 0; i < kCount; ++i) {
        _axes[i].bind(console, show_patch, i);
    }
}

void AxisProxyTable::syncAll() noexcept
{
    for (uint8_t i = 0; i < kCount; ++i) {
        _axes[i].syncFromPatch();
    }
}

AxisProxy& AxisProxyTable::operator[](uint8_t i) noexcept
{
    return _axes[i < kCount ? i : 0u];
}

const AxisProxy& AxisProxyTable::operator[](uint8_t i) const noexcept
{
    return _axes[i < kCount ? i : 0u];
}

AxisProxy* AxisProxyTable::byId(uint16_t id) noexcept
{
    Patch* p = _show->patch.byId(id);
    if (p == nullptr) {
        return nullptr;
    }
    for (uint8_t i = 0; i < kCount; ++i) {
        if (_show->patch.get(i) == p) {
            return &_axes[i];
        }
    }
    return nullptr;
}

void AxisProxyTable::onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept
{
    for (uint8_t i = 0; i < kCount; ++i) {
        _axes[i].onTelemetry(src_id, telemetry);
    }
}

void AxisProxyTable::clearLive() noexcept
{
    for (uint8_t i = 0; i < kCount; ++i) {
        _axes[i].clearLive();
    }
}

void AxisProxyTable::clearProfiles(uint8_t server_id) noexcept
{
    for (uint8_t i = 0; i < kCount; ++i) {
        if (_axes[i].route().server_id == server_id) {
            _axes[i].setProfile(nullptr);
        }
    }
}

} // namespace show
} // namespace smcp
