/**
 * @file console.cpp
 * @brief Axis + Console: master Session TX, Telemetry RX.
 */

#include "segment/console.hpp"

namespace smcp::segment {

namespace console {

Axis::Axis(Console& console) noexcept
    : _console(&console)
{}

bool Axis::select(uint8_t console_id) noexcept
{
    const axis::Route r = route();
    if (r.empty()) {
        return false;
    }
    segment::Selection sel;
    sel.add(r.axis_id);
    Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->select(console_id == kHolderNone ? segment::Action::Remove
                                               : segment::Action::Add,
                     sel);
}

bool Axis::block(bool blocked) noexcept
{
    const axis::Route r = route();
    if (r.empty()) {
        return false;
    }
    segment::Selection sel;
    sel.add(r.axis_id);
    Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->block(blocked ? segment::Action::Add : segment::Action::Remove, sel);
}

bool Axis::setTarget(const axis::MotionTarget& target) noexcept
{
    const axis::Route r = route();
    if (r.empty()) {
        return false;
    }
    Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->setTarget(r.axis_id, target);
}

bool Axis::resetFault() noexcept
{
    const axis::Route r = route();
    if (r.empty()) {
        return false;
    }
    Session* const s = _console->session(r.server_id);
    if (s == nullptr) {
        return false;
    }
    return s->resetFault(segment::Selection::of(r.axis_id));
}

void Axis::onTelemetry(const Telemetry& telemetry) noexcept
{
    _holder = telemetry.holder_id;
    _position = telemetry.position;
    _status = telemetry.status;
}

} // namespace console

Console::Console(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

void Console::begin(uint8_t console_id) noexcept
{
    if (!segment::isConsoleId(console_id)) {
        return;
    }
    Node::begin(console_id);
}

void Console::end() noexcept
{
    Node::end();
}

console::Session* Console::session(uint8_t server_id) noexcept
{
    return static_cast<console::Session*>(sessionByPeer(server_id));
}

console::Session* Console::start(uint8_t server_id) noexcept
{
    if (!isReady() || !segment::isServerId(server_id)) {
        return nullptr;
    }

    ::smcp::Session* existing = sessionByPeer(server_id);
    if (existing != nullptr) {
        if (existing->getStatus() == ::smcp::Session::Status::Idle) {
            existing->start(server_id);
        }
        return static_cast<console::Session*>(existing);
    }

    ::smcp::Session* const slot = Node::idleSession();
    if (slot == nullptr) {
        onSessionFull(server_id);
        return nullptr;
    }
    slot->start(server_id);
    return static_cast<console::Session*>(slot);
}

namespace console {

Session::Session(Console& console) noexcept
    : ::smcp::Session(console)
{}

bool Session::onPacket(const Packet& pkt) noexcept
{
    if (::smcp::Session::onPacket(pkt)) {
        return true;
    }
    SMCP_IF_MSG(Telemetry) {
        if (!isOpen()) {
            return false;
        }
        return onTelemetry(body);
    }
    return false;
}

bool Session::onTelemetry(const Telemetry& body) noexcept
{
    const uint8_t src = peerId();
    bool hit = false;
    console().axes().forEach([&](Axis& a, uint16_t) noexcept -> bool {
        const axis::Route r = a.route();
        if (r.server_id == src && r.axis_id == body.axis_id) {
            a.onTelemetry(body);
            hit = true;
            return true; /* стоп */
        }
        return false;
    });
    return hit;
}

session::TaggedOps::Result Session::getAxisRegistered() noexcept
{
    IBuf buf{reinterpret_cast<uint8_t*>(&_axis_registered),
             sizeof(AxisRegisteredInfo)};
    return get(segment::ObjTag::AxisRegistered, buf, segment::ObjTag::kIndexAll);
}

session::TaggedOps::Result Session::getAxisProfile(uint8_t axis_id,
                                             axis::Profile& out) noexcept
{
    if (axis_id > axis::kAxisIdMax) {
        return session::TaggedOps::Result::Fail;
    }
    IBuf buf{reinterpret_cast<uint8_t*>(&out),
             static_cast<uint16_t>(sizeof(axis::Profile))};
    return get(segment::ObjTag::AxisProfile, buf, axis_id);
}

void Session::onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                            session::MultiByte::Error status) noexcept
{
    (void)index;
    if (status != session::MultiByte::Error::None) {
        return;
    }
    if (tag == segment::ObjTag::AxisRegistered) {
        if (buf.data == nullptr
            || buf.size < sizeof(AxisRegisteredInfo)) {
            return;
        }
        if (buf.data != reinterpret_cast<uint8_t*>(&_axis_registered)) {
            std::memcpy(&_axis_registered, buf.data,
                        sizeof(AxisRegisteredInfo));
        }
        return;
    }
    /* AxisProfile: тело уже в out вызывающего getAxisProfile. */
}

bool Session::select(segment::Action action, segment::Selection selection) noexcept
{
    if (selection.empty() && action != segment::Action::Set) {
        return false;
    }
    if (!isOpen()) {
        return false;
    }
    segment::Select body{};
    body.action = action;
    body.selection = selection;
    return request(body);
}

bool Session::setSelection(segment::Selection selection) noexcept
{
    return select(segment::Action::Set, selection);
}

bool Session::clearSelection() noexcept
{
    return setSelection(segment::Selection{});
}

bool Session::block(segment::Action action, segment::Selection selection) noexcept
{
    if (selection.empty() && action != segment::Action::Set) {
        return false;
    }
    if (!isOpen()) {
        return false;
    }
    segment::Block body{};
    body.action = action;
    body.selection = selection;
    return request(body);
}

bool Session::setBlocked(segment::Selection selection) noexcept
{
    return block(segment::Action::Set, selection);
}

bool Session::clearBlocked() noexcept
{
    return setBlocked(segment::Selection{});
}

bool Session::setTarget(uint8_t axis_id, const axis::MotionTarget& target) noexcept
{
    if (!isOpen()) {
        return false;
    }
    segment::SetTarget body{};
    body.target = target;
    body.axis_id = axis_id;
    body.all_selected = false;
    return request(body);
}

bool Session::setTarget(const axis::MotionTarget& target) noexcept
{
    if (!isOpen()) {
        return false;
    }
    segment::SetTarget body{};
    body.target = target;
    body.all_selected = true;
    return request(body);
}

bool Session::getTelemetry(segment::Selection selection) noexcept
{
    if (!isOpen()) {
        return false;
    }
    segment::GetTelemetry body{};
    body.selection = selection;
    return request(body);
}

bool Session::resetFault(Selection selection) noexcept
{
    if (!isOpen()) {
        return false;
    }
    ResetFault body{};
    body.selection = selection;
    return request(body);
}

} // namespace console

} // namespace smcp::segment
