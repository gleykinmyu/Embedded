/**
 * @file comm_check.cpp
 * @brief Unit-тесты comm::ILink / Node / Session (слоты TX, demux, AddrConflict).
 *
 *   make -C libs/comm/test comm-check
 */

#include <cstdint>
#include <cstdio>

#include "comm/comm.hpp"

namespace {

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

/**
 * Адрес кадра (Packet::src / dst).
 * kNone — константа типа; accepts(dst) — у self (broadcast/подсеть внутри).
 */
struct addr_t {
    uint8_t v = 0;

    constexpr addr_t() = default;
    constexpr addr_t(uint8_t x) noexcept : v(x) {}

    static const addr_t kNone;

    /** Принимаем кадр с этим dst (мы / наш broadcast). */
    [[nodiscard]] constexpr bool accepts(addr_t dst) const noexcept
    {
        return dst == *this || dst == addr_t{0xFFu};
    }

    friend constexpr bool operator==(addr_t a, addr_t b) noexcept { return a.v == b.v; }
    friend constexpr bool operator!=(addr_t a, addr_t b) noexcept { return a.v != b.v; }
};
inline constexpr addr_t addr_t::kNone{};

struct Packet {
    addr_t src{};
    addr_t dst{};
    uint8_t id = 0;
};

using IProto = comm::IProto<Packet>;
using ILink = comm::ILink<Packet>;

/** Окно TX линка: слот хранит кадр и cookie. Завершение можно отдать не по порядку. */
template <uint8_t Cap = 8>
struct FakeLink : ILink {
    struct Slot {
        Packet frame{};
        void* cookie = nullptr;
        typename ILink::TxResult res = ILink::TxResult::Ok;
        bool busy = false;
        bool done = false;
    };

    Slot tx[Cap]{};
    uint8_t txN = 0;

    Packet rx[Cap]{};
    uint8_t rxR = 0;
    uint8_t rxW = 0;

    typename ILink::State st = ILink::State::Open;

    [[nodiscard]] typename ILink::State state() const noexcept override { return st; }

    [[nodiscard]] std::size_t txSpace() const noexcept override
    {
        return static_cast<std::size_t>(Cap - txN);
    }

    [[nodiscard]] std::size_t txBusy() const noexcept override { return txN; }

    [[nodiscard]] bool send(const Packet& frame, void* cookie) noexcept override
    {
        if (st != ILink::State::Open || txN >= Cap) {
            return false;
        }
        for (uint8_t i = 0; i < Cap; ++i) {
            if (tx[i].busy) {
                continue;
            }
            tx[i].frame = frame;
            tx[i].cookie = cookie;
            tx[i].res = ILink::TxResult::Ok;
            tx[i].busy = true;
            tx[i].done = true;
            ++txN;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool pollTx(Packet& frame, void*& cookie,
                              typename ILink::TxResult& result) noexcept override
    {
        for (uint8_t i = 0; i < Cap; ++i) {
            if (!tx[i].busy || !tx[i].done) {
                continue;
            }
            frame = tx[i].frame;
            cookie = tx[i].cookie;
            result = tx[i].res;
            tx[i] = {};
            --txN;
            return true;
        }
        return false;
    }

    void abortTx() noexcept override
    {
        for (uint8_t i = 0; i < Cap; ++i) {
            if (tx[i].busy) {
                tx[i].res = ILink::TxResult::Failed;
                tx[i].done = true;
            }
        }
    }

    /** Пометить слот с этим cookie завершённым раньше остальных (out-of-order). */
    void complete(void* cookie) noexcept
    {
        for (uint8_t i = 0; i < Cap; ++i) {
            if (tx[i].busy && tx[i].cookie == cookie) {
                tx[i].done = true;
            }
        }
    }

    void hold(void* cookie) noexcept
    {
        for (uint8_t i = 0; i < Cap; ++i) {
            if (tx[i].busy && tx[i].cookie == cookie) {
                tx[i].done = false;
            }
        }
    }

    [[nodiscard]] bool receive(Packet& out) noexcept override
    {
        if (st != ILink::State::Open || rxR == rxW) {
            return false;
        }
        out = rx[rxR % Cap];
        ++rxR;
        return true;
    }

    bool pushRx(const Packet& f) noexcept
    {
        if (static_cast<uint8_t>(rxW - rxR) >= Cap) {
            return false;
        }
        rx[rxW % Cap] = f;
        ++rxW;
        return true;
    }
};

struct Echo : comm::Proto<Packet> {
    uint8_t wantId = 0;
    bool haveTx = false;
    bool inFlight = false;
    bool lastOk = false;
    int rx = 0;

    explicit Echo(comm::ProtoContainer<Packet>& box, uint8_t id) noexcept
        : comm::Proto<Packet>(box)
        , wantId(id)
    {}

    [[nodiscard]] bool peekTx(Packet& out, MISC::time_t) noexcept override
    {
        if (inFlight || !haveTx) {
            return false;
        }
        out = {};
        out.dst = addr_t{wantId};
        out.id = wantId;
        inFlight = true;
        haveTx = false;
        return true;
    }

    void onTxDone(const Packet&, bool ok) noexcept override
    {
        inFlight = false;
        lastOk = ok;
    }

    [[nodiscard]] bool onPacket(const Packet& pkt, MISC::time_t) noexcept override
    {
        if (pkt.id != wantId) {
            return false;
        }
        ++rx;
        return true;
    }

    void tick(MISC::time_t) noexcept override {}
    void abort() noexcept override
    {
        haveTx = false;
        inFlight = false;
    }
};

struct TestNode : comm::Node<Packet> {
    int stateHits = 0;
    int errHits = 0;
    State lastState = State::Idle;
    Error lastError = Error::None;

    using comm::Node<Packet>::Node;
    using comm::Node<Packet>::goReady;

protected:
    void onState(State st) noexcept override
    {
        ++stateHits;
        lastState = st;
    }
    void onError(Error err) noexcept override
    {
        ++errHits;
        lastError = err;
    }
};

void test_tx_slot_and_done()
{
    std::puts("CASE Node: peekTx → send → pollTx → onTxDone");
    FakeLink<> link;
    TestNode node(link);
    comm::Session<Packet> sess(node);
    Echo echo(sess, 7);

    node.begin(1);
    node.goReady();
    sess.open(2);
    echo.haveTx = true;

    node.update(0);
    CHECK(echo.inFlight);
    CHECK(node.txBusy() == 1u);
    CHECK(link.txN == 1u && link.tx[0].frame.src == addr_t{1} && link.tx[0].frame.dst == addr_t{7});

    node.update(1); // pollTx
    CHECK(!echo.inFlight && echo.lastOk);
    CHECK(node.txBusy() == 0u);
}

void test_session_rx_demux()
{
    std::puts("CASE SessionBank: find/open + RX по peer");
    FakeLink<> link;
    TestNode node(link);
    comm::SessionBank<Packet, 2> bank(node);
    Echo ea(bank.at(0), 10);
    Echo eb(bank.at(1), 20);

    node.begin(1);
    node.goReady();
    CHECK(bank.open(2) == &bank.at(0));
    CHECK(bank.open(3) == &bank.at(1));
    CHECK(bank.find(2) == &bank.at(0));
    CHECK(bank.open(2) == &bank.at(0)); // уже open
    CHECK(bank.open(4) == nullptr);     // нет слота

    Packet pkt{};
    pkt.src = 2;
    pkt.dst = 1;
    pkt.id = 10;
    CHECK(link.pushRx(pkt));
    node.update(0);
    CHECK(ea.rx == 1 && eb.rx == 0);

    pkt.src = 3;
    pkt.id = 20;
    CHECK(link.pushRx(pkt));
    node.update(1);
    CHECK(ea.rx == 1 && eb.rx == 1);
}

void test_id_conflict()
{
    std::puts("CASE Listen: _ncp, src == наш адрес → AddrConflict");
    FakeLink<> link;
    TestNode node(link);
    comm::Ncp<Packet> ncp(node);
    CHECK(ncp.linked());
    comm::Ncp<Packet> extra(node);
    CHECK(!extra.linked());
    node.begin(5);
    CHECK(node.state() == TestNode::State::Listen);

    Packet pkt{};
    pkt.src = 5;
    pkt.dst = 1;
    CHECK(link.pushRx(pkt));
    node.update(0);
    CHECK(node.error() == TestNode::Error::AddrConflict);
    CHECK(node.state() == TestNode::State::Idle);
    CHECK(node.errHits >= 1);
}

void test_ncp_null()
{
    std::puts("CASE _ncp == nullptr: Listen без конфликта, update → Ready");
    FakeLink<> link;
    TestNode node(link);
    node.begin(5);

    Packet pkt{};
    pkt.src = 5;
    pkt.dst = 1;
    CHECK(link.pushRx(pkt));
    node.update(0);
    CHECK(node.error() == TestNode::Error::None);
    CHECK(node.state() == TestNode::State::Ready);
}

void test_listen_window()
{
    std::puts("CASE Listen: окно NCP → Ready");
    FakeLink<> link;
    TestNode node(link);
    comm::Ncp<Packet> ncp(node, 10);
    node.begin(5);

    node.update(0);
    CHECK(node.state() == TestNode::State::Listen);
    node.update(9);
    CHECK(node.state() == TestNode::State::Listen);
    node.update(10);
    CHECK(node.state() == TestNode::State::Ready);
    CHECK(node.error() == TestNode::Error::None);
}

void test_window_two()
{
    std::puts("CASE TxWindow=2: два слота");
    FakeLink<> link;
    TestNode node(link);
    comm::Session<Packet> sess(node);
    Echo a(sess, 1);
    Echo b(sess, 2);

    node.begin(9);
    node.goReady();
    sess.open(4);
    a.haveTx = true;
    b.haveTx = true;

    node.update(0);
    CHECK(node.txBusy() == 2u);
    CHECK(a.inFlight && b.inFlight);

    node.update(1);
    CHECK(node.txBusy() == 0u);
    CHECK(!a.inFlight && !b.inFlight && a.lastOk && b.lastOk);
}

void test_ooo_cookie()
{
    std::puts("CASE cookie: завершение не по порядку send");
    FakeLink<> link;
    TestNode node(link);
    comm::Session<Packet> sess(node);
    Echo a(sess, 1);
    Echo b(sess, 2);

    node.begin(9);
    node.goReady();
    sess.open(4);
    a.haveTx = true;
    b.haveTx = true;
    node.update(0);
    CHECK(node.txBusy() == 2u);

    link.hold(&a);
    node.update(1);
    CHECK(a.inFlight && !b.inFlight && b.lastOk);
    CHECK(node.txBusy() == 1u);

    link.complete(&a);
    node.update(2);
    CHECK(!a.inFlight && a.lastOk && node.txBusy() == 0u);
}

void test_closed_session_silent()
{
    std::puts("CASE закрытая Session не peekTx / не onPacket");
    FakeLink<> link;
    TestNode node(link);
    comm::Session<Packet> sess(node);
    Echo echo(sess, 3);

    node.begin(1);
    node.goReady();
    echo.haveTx = true;
    node.update(0);
    CHECK(node.txBusy() == 0u && !echo.inFlight);

    sess.open(2);
    echo.haveTx = true;
    node.update(1);
    CHECK(echo.inFlight);
    node.update(2);
    CHECK(!echo.inFlight);

    sess.close();
    Packet pkt{};
    pkt.src = 2;
    pkt.dst = 1;
    pkt.id = 3;
    CHECK(link.pushRx(pkt));
    node.update(3);
    CHECK(echo.rx == 0);
}

} // namespace

int main()
{
    std::puts("comm::ILink / Node / SessionBank");
    test_tx_slot_and_done();
    test_session_rx_demux();
    test_id_conflict();
    test_ncp_null();
    test_listen_window();
    test_window_two();
    test_ooo_cookie();
    test_closed_session_silent();
    std::printf("  ok=%d fail=%d\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
