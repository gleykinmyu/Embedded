/**
 * @file segment_check.cpp
 * @brief In-process Console ↔ Server: AxisRegistered + AxisProfile (паспорт).
 *
 *   make -C host segment-check
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hub_can.hpp"
#include "mem/range_alloc.hpp"
#include "mem/slot.hpp"
#include "segment/server.hpp"
#include "smcp/can_link.hpp"
#include "smcp/show/console.hpp"

namespace {

uint32_t g_now = 0;

uint32_t checkClock() noexcept
{
    return g_now;
}

int g_ok = 0;
int g_fail = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (cond) {                                                            \
            ++g_ok;                                                            \
        } else {                                                               \
            ++g_fail;                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                      \
    } while (0)

/** Leaf-ось для bench. */
class TestAxis : public smcp::segment::server::Axis {
public:
    using smcp::segment::server::Axis::Axis;

    [[nodiscard]] bool setTarget(const smcp::axis::MotionTarget& target) noexcept override
    {
        (void)target;
        return true;
    }
    [[nodiscard]] bool resetFault() noexcept override { return true; }
};

/** Сервер: NAxes осей, до 2 консолей. */
template <uint8_t NAxes>
class TestServer : public smcp::segment::Server<NAxes, 2> {
public:
    using Base = smcp::segment::Server<NAxes, 2>;
    using Base::kAxisCount;
    using Base::kSessionCount;
    static constexpr uint32_t kPin = 0xC0FFEEu;

    explicit TestServer(smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
        : Base(link, clock)
        , _sessionBank(*this)
        , _axes(*this)
    {
        this->setServicePinHash(kPin);
    }

    [[nodiscard]] TestAxis& mech(uint8_t id) noexcept { return _axes[id]; }

protected:
    [[nodiscard]] smcp::SessionSlots& sessions() noexcept override { return _sessionBank; }

private:
    smcp::SessionBank<kSessionCount, smcp::segment::server::Session> _sessionBank;
    smcp::segment::server::AxisBank<TestAxis, kAxisCount> _axes;
};

using TestServer4 = TestServer<4>;

/**
 * Сервер с произвольным набором phys axis_id (не обязательно 0…N-1).
 * Ёмкость inventory = 8; оси — члены leaf, register в ctor Axis.
 */
template <uint8_t... AxisIds>
class SparseServer : public smcp::segment::Server<8, 2> {
public:
    using Base = smcp::segment::Server<8, 2>;
    using Base::kSessionCount;
    static constexpr uint32_t kPin = 0xC0FFEEu;
    static constexpr uint8_t kAxisN = sizeof...(AxisIds);

    explicit SparseServer(smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
        : Base(link, clock)
        , _sessionBank(*this)
        , _axes{TestAxis{*this, AxisIds}...}
    {
        this->setServicePinHash(kPin);
    }

    [[nodiscard]] TestAxis& mech(uint8_t id) noexcept
    {
        return static_cast<TestAxis&>(*this->axis(id));
    }

    [[nodiscard]] static smcp::segment::Selection expectedMask() noexcept
    {
        smcp::segment::Selection s{};
        const uint8_t ids[] = {AxisIds...};
        for (uint8_t id : ids) {
            s.add(id);
        }
        return s;
    }

protected:
    [[nodiscard]] smcp::SessionSlots& sessions() noexcept override { return _sessionBank; }

private:
    smcp::SessionBank<kSessionCount, smcp::segment::server::Session> _sessionBank;
    TestAxis _axes[kAxisN];
};

/** 0x10: {0,5}  |  0x11: {1,2,7}  |  0x12: {0,3,4,6} */
using SparseSrvA = SparseServer<0, 5>;
using SparseSrvB = SparseServer<1, 2, 7>;
using SparseSrvC = SparseServer<0, 3, 4, 6>;

/**
 * Пульт: до 3 сессий к серверам; profile pool ≥ сумме осей на шине (2+3+4).
 */
class TestConsole : public smcp::show::Console {
public:
    static constexpr uint8_t kAxisCount = 4u;
    static constexpr uint8_t kSessionCount = 3u;
    static constexpr uint8_t kProfileCap = 32u;

    explicit TestConsole(smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
        : smcp::show::Console(link, clock)
        , _sessionBank(*this)
        , _mechs(*this)
        , _profileRanges(_profileSlots)
    {}

protected:
    [[nodiscard]] smcp::SessionSlots& sessions() noexcept override { return _sessionBank; }
    [[nodiscard]] smcp::segment::console::AxisSlots& axes() noexcept override { return _mechs; }
    [[nodiscard]] smcp::show::ProfileAllocator& profileAlloc() noexcept override
    {
        return _profileRanges;
    }

private:
    smcp::SessionBank<kSessionCount, smcp::show::Session> _sessionBank;
    smcp::segment::console::AxisStore<kAxisCount> _mechs;
    mem::SlotStore8<smcp::show::ProfileSlot, kProfileCap> _profileSlots{};
    mem::RangeAllocator8<smcp::show::ProfileSlot, kSessionCount> _profileRanges;
};

void seedAxisProfile(smcp::segment::server::Axis& axis, uint8_t server_id) noexcept
{
    const uint8_t id = axis.axisId();
    auto& p = axis.profile;
    p.clear();
    p.axis_id = id;
    char name[smcp::axis::kNameSize]{};
    std::snprintf(name, sizeof(name), "S%02X.a%u",
                  static_cast<unsigned>(server_id),
                  static_cast<unsigned>(id));
    p.setName(name);
    /* Уникальный паспорт: server_id + axis_id. */
    p.limits.pos.min = static_cast<int32_t>(server_id) * 1000 + id * 17;
    p.limits.pos.max = static_cast<int32_t>(server_id) * 1000 + 500 + id * 17;
    p.limits.speed.max = static_cast<uint16_t>(256u * server_id + id);
    p.limits.accel.max = static_cast<uint16_t>(10u + id);
}

template <typename Srv>
void seedProfiles(Srv& srv, uint8_t server_id)
{
    srv.axisRegistered().selection.forEach([&](uint8_t /*index*/, uint8_t id) {
        smcp::segment::server::Axis* a = srv.axis(id);
        if (a != nullptr) {
            seedAxisProfile(*a, server_id);
        }
        return false;
    });
}

[[nodiscard]] bool profilesEqual(const smcp::axis::Profile& a,
                                 const smcp::axis::Profile& b) noexcept
{
    return a.axis_id == b.axis_id
        && a.limits.pos.min == b.limits.pos.min
        && a.limits.pos.max == b.limits.pos.max
        && a.limits.speed.max == b.limits.speed.max
        && a.limits.accel.max == b.limits.accel.max
        && std::strncmp(a.name, b.name, sizeof(a.name)) == 0;
}

/** 1 console + 1 server (старые CASE). */
struct Mesh {
    smcp::host::CanHub hub;
    smcp::host::HubCan cons_can{hub};
    smcp::host::HubCan srv_can{hub};
    smcp::CanLink cons_link{cons_can, 0};
    smcp::CanLink srv_link{srv_can, 0};
    TestConsole cons{cons_link, checkClock};
    TestServer4 srv{srv_link, checkClock};

    Mesh()
    {
        (void)cons_can.open(0);
        (void)srv_can.open(0);
    }

    void pump(uint32_t ms, uint32_t step_ms = 10u)
    {
        uint32_t left = ms;
        while (left > 0u) {
            const uint32_t d = left < step_ms ? left : step_ms;
            g_now += d;
            if (cons.getState() != smcp::Node::State::Idle) {
                cons.update();
            }
            if (srv.getState() != smcp::Node::State::Idle) {
                srv.update();
            }
            left -= d;
        }
    }

    void ready()
    {
        cons.begin(0x01);
        srv.begin(0x10);
        pump(smcp::session::Heartbeat::kTimeoutMs + 40u);
        CHECK(cons.isReady());
        CHECK(srv.isReady());
    }

    [[nodiscard]] bool openLink()
    {
        auto* s = cons.start(0x10);
        if (s == nullptr) {
            return false;
        }
        pump(800u);
        auto* peer = srv.sessionByPeer(0x01);
        return s->isOpen() && peer != nullptr && peer->isOpen();
    }
};

/** 2 console + 3 server с разными наборами axis_id. */
struct MultiMesh {
    static constexpr uint8_t kC1 = 0x01u;
    static constexpr uint8_t kC2 = 0x02u;
    static constexpr uint8_t kSA = 0x10u; /* {0,5} */
    static constexpr uint8_t kSB = 0x11u; /* {1,2,7} */
    static constexpr uint8_t kSC = 0x12u; /* {0,3,4,6} */

    smcp::host::CanHub hub;
    smcp::host::HubCan c1_can{hub};
    smcp::host::HubCan c2_can{hub};
    smcp::host::HubCan sa_can{hub};
    smcp::host::HubCan sb_can{hub};
    smcp::host::HubCan sc_can{hub};
    smcp::CanLink c1_link{c1_can, 0};
    smcp::CanLink c2_link{c2_can, 0};
    smcp::CanLink sa_link{sa_can, 0};
    smcp::CanLink sb_link{sb_can, 0};
    smcp::CanLink sc_link{sc_can, 0};
    TestConsole c1{c1_link, checkClock};
    TestConsole c2{c2_link, checkClock};
    SparseSrvA sa{sa_link, checkClock};
    SparseSrvB sb{sb_link, checkClock};
    SparseSrvC sc{sc_link, checkClock};

    MultiMesh()
    {
        (void)c1_can.open(0);
        (void)c2_can.open(0);
        (void)sa_can.open(0);
        (void)sb_can.open(0);
        (void)sc_can.open(0);
    }

    void pump(uint32_t ms, uint32_t step_ms = 10u)
    {
        uint32_t left = ms;
        while (left > 0u) {
            const uint32_t d = left < step_ms ? left : step_ms;
            g_now += d;
            if (c1.getState() != smcp::Node::State::Idle) {
                c1.update();
            }
            if (c2.getState() != smcp::Node::State::Idle) {
                c2.update();
            }
            if (sa.getState() != smcp::Node::State::Idle) {
                sa.update();
            }
            if (sb.getState() != smcp::Node::State::Idle) {
                sb.update();
            }
            if (sc.getState() != smcp::Node::State::Idle) {
                sc.update();
            }
            left -= d;
        }
    }

    void ready()
    {
        c1.begin(kC1);
        c2.begin(kC2);
        sa.begin(kSA);
        sb.begin(kSB);
        sc.begin(kSC);
        pump(smcp::session::Heartbeat::kTimeoutMs + 40u);
        CHECK(c1.isReady());
        CHECK(c2.isReady());
        CHECK(sa.isReady());
        CHECK(sb.isReady());
        CHECK(sc.isReady());
    }

    [[nodiscard]] bool openPair(TestConsole& cons, uint8_t console_id, uint8_t server_id,
                                smcp::Node& srv)
    {
        auto* s = cons.start(server_id);
        if (s == nullptr) {
            return false;
        }
        pump(800u);
        auto* peer = srv.sessionByPeer(console_id);
        return s->isOpen() && peer != nullptr && peer->isOpen();
    }
};

void waitReady(MultiMesh& m, smcp::show::Session* sess)
{
    m.pump(4000u);
    CHECK(sess != nullptr);
    CHECK(sess->fetchState() == smcp::show::FetchState::Ready);
}

void checkSameProfiles(smcp::show::Session& a, smcp::show::Session& b,
                       smcp::segment::Selection want,
                       smcp::segment::IServer& srv)
{
    CHECK(a.axisRegistered().selection.raw() == want.raw());
    CHECK(b.axisRegistered().selection.raw() == want.raw());
    CHECK(a.axisRegistered().selection.count() == want.count());
    CHECK(a.axisRegistered().revision == b.axisRegistered().revision);

    want.forEach([&](uint8_t /*index*/, uint8_t id) {
        const smcp::axis::Profile* pa = a.profile(id);
        const smcp::axis::Profile* pb = b.profile(id);
        const smcp::segment::server::Axis* ax = srv.axis(id);
        CHECK(pa != nullptr);
        CHECK(pb != nullptr);
        CHECK(ax != nullptr);
        CHECK(pa->isValid());
        CHECK(profilesEqual(*pa, *pb));
        CHECK(profilesEqual(*pa, ax->profile));
        return false;
    });

    /* Чужой id на этом сервере — слота нет. */
    for (uint8_t id = 0; id < smcp::axis::kAxisCount; ++id) {
        if (want.contains(id)) {
            continue;
        }
        CHECK(a.profile(id) == nullptr);
        CHECK(b.profile(id) == nullptr);
    }
}

void case_axis_registered_and_profiles()
{
    std::puts("CASE AxisRegistered + auto Get AxisProfile");
    Mesh m;
    m.ready();
    seedProfiles(m.srv, 0x10);
    CHECK(m.openLink());

    auto* sess = m.cons.session(0x10);
    CHECK(sess != nullptr);

    CHECK(sess->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    m.pump(300u);
    const smcp::segment::Selection reg = sess->axisRegistered().selection;
    CHECK(reg.count() == TestServer4::kAxisCount);
    for (uint8_t i = 0; i < TestServer4::kAxisCount; ++i) {
        CHECK(reg.contains(i));
    }

    m.pump(2500u);
    CHECK(sess->fetchState() == smcp::show::FetchState::Ready);

    const smcp::axis::Profile* p0 = sess->profile(0);
    CHECK(p0 != nullptr);
    CHECK(p0->limits.pos.min == static_cast<int32_t>(0x10) * 1000);
    CHECK(p0->limits.speed.max == static_cast<uint16_t>(256u * 0x10));
    CHECK(std::strncmp(p0->name, "S10.a0", sizeof(p0->name)) == 0);

    const smcp::axis::Profile* p2 = sess->profile(2);
    CHECK(p2 != nullptr);
    CHECK(p2->limits.pos.min == static_cast<int32_t>(0x10) * 1000 + 2 * 17);
    CHECK(p2->limits.speed.max == static_cast<uint16_t>(256u * 0x10 + 2));
}

void case_profile_all_nack()
{
    std::puts("CASE Get AxisProfile kIndexAll → Nack NotSupported");
    Mesh m;
    m.ready();
    CHECK(m.openLink());
    auto* sess = m.cons.session(0x10);
    CHECK(sess != nullptr);

    uint8_t scratch[sizeof(smcp::axis::Profile)]{};
    smcp::IBuf buf{scratch, sizeof(scratch)};
    CHECK(sess->get(smcp::segment::ObjTag::AxisProfile, buf,
                    smcp::segment::ObjTag::kIndexAll)
          == smcp::session::TaggedOps::Result::Ok);
    m.pump(300u);
    CHECK(sess->profile(smcp::segment::ObjTag::kIndexAll) == nullptr);
}

void case_profile_missing_axis()
{
    std::puts("CASE Get AxisProfile без AxisRegistered / вне inventory");
    Mesh m;
    m.ready();
    CHECK(m.openLink());
    auto* sess = m.cons.session(0x10);
    CHECK(sess != nullptr);

    CHECK(sess->fetchAxisProfile(7) == smcp::session::TaggedOps::Result::Fail);
    CHECK(sess->profile(7) == nullptr);

    CHECK(sess->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    m.pump(300u);
    CHECK(sess->fetchAxisProfile(7) == smcp::session::TaggedOps::Result::Fail);
    CHECK(sess->profile(7) == nullptr);
}

void case_set_profile_with_pin()
{
    std::puts("CASE ServiceAccess + Set AxisProfile + Get");
    Mesh m;
    m.ready();
    seedProfiles(m.srv, 0x10);
    CHECK(m.openLink());
    auto* sess = m.cons.session(0x10);
    CHECK(sess != nullptr);

    CHECK(sess->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    m.pump(2500u);
    CHECK(sess->fetchState() == smcp::show::FetchState::Ready);
    const uint16_t rev0 = sess->axisRegistered().revision;
    CHECK(m.srv.axisRegistered().revision == rev0);

    CHECK(sess->request(smcp::segment::ServiceAccess::makeUnlock(TestServer4::kPin)));
    m.pump(200u);
    CHECK(m.srv.isServiceUnlocked());

    smcp::axis::Profile neu{};
    neu.setName("New");
    neu.limits.pos.min = -5;
    neu.limits.pos.max = 55;
    neu.limits.speed.max = 99u;
    smcp::IBuf body{reinterpret_cast<uint8_t*>(&neu),
                    static_cast<uint16_t>(sizeof(neu))};
    CHECK(sess->set(smcp::segment::ObjTag::AxisProfile, body, 1u) == smcp::session::TaggedOps::Result::Ok);
    m.pump(600u);
    CHECK(m.srv.mech(1).profile.limits.speed.max == 99u);
    CHECK(std::strncmp(m.srv.mech(1).profile.name, "New", 4) == 0);
    CHECK(m.srv.axisRegistered().revision == static_cast<uint16_t>(rev0 + 1u));
    m.pump(2500u);
    CHECK(sess->axisRegistered().revision == m.srv.axisRegistered().revision);
    CHECK(sess->fetchState() == smcp::show::FetchState::Ready);
    const smcp::axis::Profile* got = sess->profile(1);
    CHECK(got != nullptr);
    CHECK(got->limits.speed.max == 99u);
    CHECK(got->limits.pos.max == 55);
}

void case_set_without_pin_rejected()
{
    std::puts("CASE Set AxisProfile without ServiceAccess → reject");
    Mesh m;
    m.ready();
    seedProfiles(m.srv, 0x10);
    CHECK(m.openLink());
    auto* sess = m.cons.session(0x10);
    CHECK(sess != nullptr);

    const uint16_t before = m.srv.mech(0).profile.limits.speed.max;
    smcp::axis::Profile neu{};
    neu.limits.speed.max = 123u;
    smcp::IBuf body{reinterpret_cast<uint8_t*>(&neu),
                    static_cast<uint16_t>(sizeof(neu))};
    CHECK(sess->set(smcp::segment::ObjTag::AxisProfile, body, 0u) == smcp::session::TaggedOps::Result::Ok);
    m.pump(600u);
    CHECK(m.srv.mech(0).profile.limits.speed.max == before);
}

void case_two_consoles_three_servers()
{
    std::puts("CASE 2 consoles × 3 servers (sparse axis sets) — same profiles");
    MultiMesh m;
    m.ready();
    seedProfiles(m.sa, MultiMesh::kSA);
    seedProfiles(m.sb, MultiMesh::kSB);
    seedProfiles(m.sc, MultiMesh::kSC);

    CHECK(m.sa.axisRegistered().selection.raw() == SparseSrvA::expectedMask().raw());
    CHECK(m.sb.axisRegistered().selection.raw() == SparseSrvB::expectedMask().raw());
    CHECK(m.sc.axisRegistered().selection.raw() == SparseSrvC::expectedMask().raw());
    /* Наборы не совпадают между серверами. */
    CHECK(SparseSrvA::expectedMask().raw() != SparseSrvB::expectedMask().raw());
    CHECK(SparseSrvB::expectedMask().raw() != SparseSrvC::expectedMask().raw());
    CHECK(SparseSrvA::expectedMask().raw() != SparseSrvC::expectedMask().raw());

    CHECK(m.openPair(m.c1, MultiMesh::kC1, MultiMesh::kSA, m.sa));
    CHECK(m.openPair(m.c1, MultiMesh::kC1, MultiMesh::kSB, m.sb));
    CHECK(m.openPair(m.c1, MultiMesh::kC1, MultiMesh::kSC, m.sc));
    CHECK(m.openPair(m.c2, MultiMesh::kC2, MultiMesh::kSA, m.sa));
    CHECK(m.openPair(m.c2, MultiMesh::kC2, MultiMesh::kSB, m.sb));
    CHECK(m.openPair(m.c2, MultiMesh::kC2, MultiMesh::kSC, m.sc));

    auto* c1a = m.c1.session(MultiMesh::kSA);
    auto* c1b = m.c1.session(MultiMesh::kSB);
    auto* c1c = m.c1.session(MultiMesh::kSC);
    auto* c2a = m.c2.session(MultiMesh::kSA);
    auto* c2b = m.c2.session(MultiMesh::kSB);
    auto* c2c = m.c2.session(MultiMesh::kSC);
    CHECK(c1a && c1b && c1c && c2a && c2b && c2c);

    CHECK(c1a->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    CHECK(c1b->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    CHECK(c1c->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    CHECK(c2a->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    CHECK(c2b->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);
    CHECK(c2c->getAxisRegistered() == smcp::session::TaggedOps::Result::Ok);

    waitReady(m, c1a);
    waitReady(m, c1b);
    waitReady(m, c1c);
    waitReady(m, c2a);
    waitReady(m, c2b);
    waitReady(m, c2c);

    checkSameProfiles(*c1a, *c2a, SparseSrvA::expectedMask(), m.sa);
    checkSameProfiles(*c1b, *c2b, SparseSrvB::expectedMask(), m.sb);
    checkSameProfiles(*c1c, *c2c, SparseSrvC::expectedMask(), m.sc);
}

} // namespace

int main()
{
    std::puts("SMCP segment — Console↔Server axis passport");
    case_axis_registered_and_profiles();
    case_profile_all_nack();
    case_profile_missing_axis();
    case_set_profile_with_pin();
    case_set_without_pin_rejected();
    case_two_consoles_three_servers();

    std::printf("\n%d ok, %d fail\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
