/**
 * @file axis_proxy.cpp
 * @brief AxisProxy / AxisProxyTable: TX через Console, зеркало Telemetry.
 */

#include "smcp/show/axis_proxy.hpp"

#include "segment/console.hpp"
#include "smcp/show/console.hpp"

namespace smcp {
namespace show {

void AxisProxy::bind(segment::Console& console, Patch<>& show_patch, uint8_t slot_index) noexcept
{
    _console = &console;
    _show = &show_patch;
    _slot = slot_index;
    syncFromPatch();
}

void AxisProxy::syncFromPatch() noexcept
{
    PatchRec* const p = patch();
    if (p == nullptr || !p->isRouted()) {
        _axis_id = 0;
        _type = nullptr;
        return;
    }
    _axis_id = p->route.axis_id;
    _type = _show != nullptr ? _show->typeOf(*p) : nullptr;
}

bool AxisProxy::isBound() const noexcept
{
    return _console != nullptr && _show != nullptr;
}

bool AxisProxy::isEmpty() const noexcept
{
    const PatchRec* p = patch();
    return p == nullptr || p->isEmpty();
}

bool AxisProxy::isRouted() const noexcept
{
    const PatchRec* p = patch();
    return p != nullptr && p->isRouted();
}

PatchRec* AxisProxy::patch() noexcept
{
    return _show != nullptr ? _show->recs.get(_slot) : nullptr;
}

const PatchRec* AxisProxy::patch() const noexcept
{
    return _show != nullptr ? _show->recs.get(_slot) : nullptr;
}

axis::Route AxisProxy::route() const noexcept
{
    const PatchRec* p = patch();
    return p != nullptr ? p->route : axis::Route{};
}

uint16_t AxisProxy::showId() const noexcept
{
    const PatchRec* p = patch();
    return p != nullptr ? p->id : kIdNone;
}

const char* AxisProxy::name() const noexcept
{
    const PatchRec* p = patch();
    return p != nullptr ? p->name : "";
}

bool AxisProxy::select(uint8_t console_id) noexcept
{
    if (!isRouted() || _console == nullptr) {
        return false;
    }
    const axis::Route r = route();
    segment::Selection sel;
    sel.add(r.axis_id);
    segment::console::Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->select(console_id == kHolderNone ? segment::Action::Remove
                                               : segment::Action::Add,
                     sel);
}

bool AxisProxy::block(bool blocked) noexcept
{
    if (!isRouted() || _console == nullptr) {
        return false;
    }
    const axis::Route r = route();
    segment::Selection sel;
    sel.add(r.axis_id);
    segment::console::Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->block(blocked ? segment::Action::Add : segment::Action::Remove, sel);
}

bool AxisProxy::setTarget(const axis::MotionTarget& target) noexcept
{
    if (!isRouted() || _console == nullptr) {
        return false;
    }
    const axis::Route r = route();
    segment::console::Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->setTarget(r.axis_id, target);
}

bool AxisProxy::resetFault() noexcept
{
    if (!isRouted() || _console == nullptr) {
        return false;
    }
    const axis::Route r = route();
    segment::console::Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->resetFault(segment::Selection::of(r.axis_id));
}

const axis::Profile* AxisProxy::profile() const noexcept
{
    if (!isRouted() || _console == nullptr) {
        return nullptr;
    }
    const axis::Route r = route();
    auto* const s = static_cast<Session*>(_console->session(r.server_id));
    if (s == nullptr) {
        return nullptr;
    }
    return s->profile(r.axis_id);
}

void AxisProxy::onTelemetry(uint8_t src_id, const segment::Telemetry& telemetry) noexcept
{
    if (!isRouted()) {
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
}

AxisProxyTable::AxisProxyTable(Patch<>& show_patch, segment::Console& console) noexcept
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
    PatchRec* p = _show->recs.byId(id);
    if (p == nullptr) {
        return nullptr;
    }
    for (uint8_t i = 0; i < kCount; ++i) {
        if (_show->recs.get(i) == p) {
            return &_axes[i];
        }
    }
    return nullptr;
}

void AxisProxyTable::onTelemetry(uint8_t src_id, const segment::Telemetry& telemetry) noexcept
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
    (void)server_id;
}

} // namespace show
} // namespace smcp
