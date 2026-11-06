/**
 * @file console.cpp
 * @brief Axis (шкала / setTarget / Telemetry) + Session / Console: кэш Profile.
 */

#include "smcp/show/console.hpp"

namespace smcp::show {

void Axis::bindPatch(const PatchRec* rec, const AxisType* type) noexcept
{
    _patch = rec;
    _type = (rec != nullptr) ? type : nullptr;
    refreshScale();
}

bool ProfileSlot::bind(Axis& a) noexcept
{
    if (axis == &a) {
        return false;
    }
    Axis* const prev = axis;
    axis = &a;
    if (prev != nullptr) {
        prev->bindProfile(nullptr);
    }
    a.bindProfile(this);
    return true;
}

void ProfileSlot::unbind() noexcept
{
    if (axis == nullptr) {
        return;
    }
    Axis* const prev = axis;
    axis = nullptr;
    prev->bindProfile(nullptr);
}

void ProfileSlot::refreshAxis() noexcept
{
    if (axis != nullptr) {
        axis->refreshScale();
    }
}

void Axis::bindProfile(ProfileSlot* slot) noexcept
{
    if (_slot == slot) {
        return;
    }
    if (_slot != nullptr && _slot->axis == this) {
        _slot->axis = nullptr;
    }
    _slot = slot;
    if (_slot != nullptr) {
        _slot->axis = this;
    }
    refreshScale();
}

void Axis::reset() noexcept
{
    bindProfile(nullptr);
    bindPatch(nullptr);
    setPatchState(PatchState::Idle);
}

void Axis::refreshScale() noexcept
{
    _scale_d = 0;
    _scale_ok = false;
    const axis::Profile* const p = profile();
    if (p == nullptr || !p->isValid()) {
        return;
    }
    axis::UnitScale con_scale = p->scale;
    if (_type != nullptr) {
        if (!_type->scale.isValid() || !_type->kind.isValid()) {
            return;
        }
        if (_type->kind != p->kind) {
            return;
        }
        con_scale = _type->scale;
    }
    _scale_d = static_cast<int8_t>(axis::scalePow(p->scale) - axis::scalePow(con_scale));
    _scale_ok = true;
}

uint16_t Axis::localId() const noexcept
{
    return (_patch != nullptr) ? _patch->id : kIdNone;
}

axis::Route Axis::route() const noexcept
{
    return (_patch != nullptr) ? _patch->route : axis::Route{};
}

bool Axis::setTarget(const axis::MotionTarget& target) noexcept
{
    if (!scaleReady()) {
        return false;
    }

    axis::MotionTarget wire = target;
    int32_t con = target.target;
    const bool invert =
        (_patch != nullptr) && _patch->flag.any(PatchFlag::Invert);
    if (invert) {
        con = -con;
    }
    if (target.mode == axis::MoveMode::Absolute) {
        const int32_t off = (_patch != nullptr) ? _patch->pos_offset : 0;
        con += off;
    }
    wire.target = toServer(con);
    wire.speed = toServer(target.speed);
    return segment::console::Axis::setTarget(wire);
}

void Axis::onTelemetry(const segment::Telemetry& telemetry) noexcept
{
    _holder = telemetry.holder_id;
    _status = telemetry.status;
    if (!scaleReady()) {
        _position = telemetry.position;
        return;
    }

    int32_t con = toConsole(telemetry.position);
    const int32_t off = (_patch != nullptr) ? _patch->pos_offset : 0;
    con -= off;
    if ((_patch != nullptr) && _patch->flag.any(PatchFlag::Invert)) {
        con = -con;
    }
    _position = con;
}

Session::Session(Console& console) noexcept
    : segment::console::Session(console)
{}

Console& Session::console() noexcept
{
    return static_cast<Console&>(segment::console::Session::console());
}

ProfileSlot* Session::findSlot(uint8_t axis_id) noexcept
{
    if (_profiles == nullptr) {
        return nullptr;
    }
    ProfileSlot* found = nullptr;
    _profiles->forEach([&](ProfileSlot& e, ProfileAllocator::idx_t) -> bool {
        if (e.profile.axis_id == axis_id) {
            found = &e;
            return true; /* стоп */
        }
        return false;
    });
    return found;
}

const ProfileSlot* Session::findSlot(uint8_t axis_id) const noexcept
{
    return const_cast<Session*>(this)->findSlot(axis_id);
}

void Session::applyRegistered() noexcept
{
    using cap_t = ProfileAllocator::cap_t;
    const segment::AxisRegisteredInfo& cur = _axis_registered;
    if ((_fetch_state == FetchState::Fetching || _fetch_state == FetchState::Ready)
        && cur == _mapped) {
        return;
    }
    /* Любая смена маски/revision — новый набор слотов; паспорта только после Get. */
    dropProfileRange();
    const cap_t n = cur.selection.count();
    if (n == 0u) {
        _mapped = cur;
        setFetchState(FetchState::Ready);
        return;
    }
    _profiles = console().profileAlloc().alloc(n);
    if (_profiles == nullptr) {
        setFetchState(FetchState::AllocFailed);
        return; /* _mapped не трогаем — повторный AxisRegistered сможет retry */
    }
    /* alloc(n) + ровно n включённых id → at(index) всегда валиден. */
    cur.selection.forEach([&](uint8_t index, uint8_t id) {
        _profiles->at(index)->profile.markDirty(id);
    });
    _mapped = cur;
    setFetchState(FetchState::Fetching);
}

void Session::dropProfileRange() noexcept
{
    _fetch_slot = nullptr;
    _fetch_fails = 0;
    if (_profiles != nullptr) {
        _profiles->forEach([](ProfileSlot& e, ProfileAllocator::idx_t) {
            e.unbind();
        });
        _profiles->free();
        _profiles = nullptr;
    }
    _fetch_state = FetchState::Idle; /* без хука */
}

const axis::Profile* Session::profile(uint8_t axis_id) const noexcept
{
    const ProfileSlot* const e = findSlot(axis_id);
    return e != nullptr ? &e->profile : nullptr;
}

void Session::releaseProfiles() noexcept
{
    dropProfileRange();
    _axis_registered = segment::AxisRegisteredInfo{};
    _mapped = segment::AxisRegisteredInfo{};
}

void Session::requestRegistered() noexcept
{
    if (!isOpen() || _fetch_state == FetchState::Fetching) {
        return;
    }
    (void)getAxisRegistered();
}

session::TaggedOps::Result Session::fetchAxisProfile(ProfileSlot& slot) noexcept
{
    const session::TaggedOps::Result st = segment::console::Session::getAxisProfile(
        slot.profile.axis_id, slot.profile);
    if (st != session::TaggedOps::Result::Ok) {
        return st; /* Busy / NotOpen / … — повторить на tick */
    }
    _fetch_slot = &slot;
    return session::TaggedOps::Result::Ok;
}

session::TaggedOps::Result Session::fetchAxisProfile(uint8_t axis_id) noexcept
{
    ProfileSlot* const e = findSlot(axis_id);
    if (e == nullptr) {
        return session::TaggedOps::Result::Fail; /* только после AxisRegistered */
    }
    return fetchAxisProfile(*e);
}

ProfileSlot* Session::nextPendingSlot() noexcept
{
    if (_profiles == nullptr) {
        return nullptr;
    }
    ProfileSlot* need = nullptr;
    _profiles->forEach([&](ProfileSlot& e, ProfileAllocator::idx_t) -> bool {
        if (!e.profile.isValid()) {
            need = &e;
            return true; /* стоп */
        }
        return false;
    });
    return need;
}

void Session::setFetchState(FetchState state, uint8_t axis_id) noexcept
{
    if (_fetch_state == state && state != FetchState::FetchFailed) {
        return;
    }
    _fetch_state = state;
    if (state == FetchState::Idle || state == FetchState::Fetching) {
        return;
    }
    console().onAxisProfile(this, state, axis_id);
}

void Session::noteFetchFail(uint8_t axis_id) noexcept
{
    ++_fetch_fails;
    if (_fetch_fails < kFetchFailLimit) {
        return;
    }
    _fetch_fails = 0;
    setFetchState(FetchState::FetchFailed, axis_id);
}

bool Session::onTelemetry(const segment::Telemetry& body) noexcept
{
    if (_profiles == nullptr) {
        return false;
    }
    ProfileSlot* const e = findSlot(body.axis_id);
    if (e == nullptr || e->axis == nullptr || !e->profile.isValid()) {
        return false;
    }
    e->axis->onTelemetry(body);
    return true;
}

void Session::tick() noexcept
{
    ::smcp::Session::tick();
    if (_fetch_state != FetchState::Fetching || !isOpen() || _profiles == nullptr) {
        return;
    }
    if (_fetch_slot != nullptr) {
        return; /* один in-flight; зависание RX → MB Timeout → onReply fail */
    }
    ProfileSlot* const need = nextPendingSlot();
    if (need == nullptr) {
        setFetchState(FetchState::Ready);
        return;
    }
    /* Start!=Ok (Busy/…) — не считаем fail; сбои ответа — onReply → noteFetchFail. */
    if (fetchAxisProfile(*need) != session::TaggedOps::Result::Ok) {
        return;
    }
}

void Session::onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                          session::MultiByte::Error status) noexcept
{
    segment::console::Session::onReply(tag, index, buf, status);
    if (tag == segment::ObjTag::AxisRegistered) {
        if (status == session::MultiByte::Error::None) {
            applyRegistered();
        }
        return;
    }
    if (tag != segment::ObjTag::AxisProfile) {
        return;
    }

    ProfileSlot* const e = _fetch_slot;
    _fetch_slot = nullptr;
    if (e == nullptr) {
        return;
    }

    if (status != session::MultiByte::Error::None
        || index > axis::kAxisIdMax
        || buf.data == nullptr
        || buf.size < sizeof(axis::Profile)) {
        /* Wire/MB сбой (в т.ч. Timeout): повтор до kFetchFailLimit. */
        const uint8_t id =
            (index <= axis::kAxisIdMax) ? index : e->profile.axis_id;
        e->profile.markDirty(id);
        e->refreshAxis();
        noteFetchFail(id);
        return;
    }

    /* Get писал in-place в e->profile; axis_id с wire мог соврать.
     * Axis не биндим здесь — bind после Patch (onAxisProfile Ready → leaf). */
    e->profile.axis_id = index;
    e->refreshAxis();
    _fetch_fails = 0;
    if (nextPendingSlot() == nullptr) {
        setFetchState(FetchState::Ready);
    }
}

void Console::resetStates() noexcept
{
    _patch_axis_total = 0;
    _patch_axis_ready = 0;
    axes().forEach([](segment::console::Axis& raw, segment::console::idx_t) noexcept {
        static_cast<Axis&>(raw).reset();
    });
}

void Console::setAxisPatchState(Axis& axis, PatchState s) noexcept
{
    const PatchState prev = axis.patchState();
    if (prev == s) {
        return;
    }
    axis.setPatchState(s);
    if (!isPatchResolved(prev) && isPatchResolved(s)) {
        ++_patch_axis_ready;
        if (_patch_axis_total > 0u && _patch_axis_ready >= _patch_axis_total) {
            onAxisPatch();
        }
    }
}

void Session::tryBindProfile(Axis& axis) noexcept
{
    const PatchRec* const rec = axis.patch();
    if (rec == nullptr || !rec->isRouted() || rec->route.server_id != peerId()) {
        return;
    }
    Console& cons = console();
    if (!isOpen() || _fetch_state != FetchState::Ready) {
        cons.setAxisPatchState(axis, PatchState::Patched);
        axis.bindProfile(nullptr);
        return;
    }
    ProfileSlot* const slot = findSlot(rec->route.axis_id);
    if (slot == nullptr || !slot->profile.isValid()) {
        cons.setAxisPatchState(axis, PatchState::NoProfile);
        axis.bindProfile(nullptr);
        return;
    }
    if (slot->axis != nullptr && slot->axis != &axis) {
        cons.setAxisPatchState(axis, PatchState::RouteConflict);
        axis.bindProfile(nullptr);
        return;
    }
    const AxisType* const ty = axis.type();
    if (ty != nullptr && ty->kind.isValid() && slot->profile.kind.isValid()
        && ty->kind != slot->profile.kind) {
        cons.setAxisPatchState(axis, PatchState::KindMismatch);
        axis.bindProfile(nullptr);
        return;
    }
    (void)slot->bind(axis);
    cons.setAxisPatchState(axis, PatchState::Synced);
}

void Session::bindProfiles() noexcept
{
    const uint8_t sid = peerId();
    console().axes().forEach([&](segment::console::Axis& raw,
                                 segment::console::idx_t) noexcept {
        auto& a = static_cast<Axis&>(raw);
        const PatchRec* const rec = a.patch();
        if (rec == nullptr || !rec->isRouted() || rec->route.server_id != sid) {
            return;
        }
        tryBindProfile(a);
    });
}

void Console::remapAxes() noexcept
{
    resetStates();
    if (patch == nullptr) {
        onAxisPatch();
        return;
    }

    const auto cap = axisCapacity();
    segment::console::idx_t slot = 0;

    for (uint8_t pi = 0; pi < patch->axisCount(); ++pi) {
        const PatchRec* const rec = patch->rec(pi);
        if (rec == nullptr || rec->isEmpty()) {
            continue;
        }
        if (slot >= cap) {
            break; /* нет свободных Axis — partial */
        }
        Axis* const a = axis(slot++);
        if (a == nullptr) {
            continue;
        }
        a->bindPatch(rec, patch->typeOf(*rec));
        ++_patch_axis_total;

        if (!rec->isRouted()) {
            setAxisPatchState(*a, PatchState::Unpatched);
            continue;
        }

        Session* s = session(rec->route.server_id);
        if (s == nullptr) {
            s = start(rec->route.server_id);
        }
        if (s == nullptr) {
            setAxisPatchState(*a, PatchState::NoSession);
            continue;
        }
        /* Профили — Link::Up → requestRegistered → onAxisProfile(Ready). */
        setAxisPatchState(*a, PatchState::Patched);
    }

    if (_patch_axis_total == 0u) {
        onAxisPatch();
        return;
    }

    /* Догон: по сессиям, у которых маска уже Ready. */
    sessions().forEach([](::smcp::Session& raw, uint8_t) noexcept {
        auto& s = static_cast<Session&>(raw);
        if (s.fetchState() == FetchState::Ready) {
            s.bindProfiles();
        }
    });
}

void Console::onAxisProfile(Session* session, FetchState state,
                            uint8_t axis_id) noexcept
{
    (void)axis_id;
    if (session == nullptr || state != FetchState::Ready) {
        return;
    }
    static_cast<Session*>(session)->bindProfiles();
}

void Console::onLink(::smcp::Session* session, Link link) noexcept
{
    Node::onLink(session, link);
    if (link != Link::Up && session != nullptr) {
        static_cast<Session*>(session)->releaseProfiles();
    }
    if (link == Link::Up && session != nullptr && patch != nullptr) {
        /* Сессия открылась → тянем маску; bindProfiles только из onAxisProfile(Ready). */
        static_cast<Session*>(session)->requestRegistered();
    }
}

void Console::onAck(::smcp::Session* session, const TxSlot& req,
                    const session::Ack& reply) noexcept
{
    Node::onAck(session, req, reply);
    if (session == nullptr || req.msg.id != session::Set::kId) {
        return;
    }
    session::Set opener{};
    if (!opener.unpack(req.msg)
        || opener.tag != segment::ObjTag::AxisProfile) {
        return;
    }
    /* Сервер уже bumpRevision — подтянуть AxisRegisteredInfo. */
    static_cast<Session*>(session)->requestRegistered();
}

} // namespace smcp::show
