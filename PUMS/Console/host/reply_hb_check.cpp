/**
 * @file reply_hb_check.cpp
 * @brief Unit-тесты smcp::proto::Reply + LinkHb (new_smcp).
 *
 *   make -C host reply-hb-check
 */

#include <cstdint>
#include <cstdio>

#include "smcp/new_smcp/new_smcp.hpp"

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

using smcp::Frame;
using smcp::addr_t;
using smcp::MsgId;
using smcp::Ack;
using smcp::Nack;
using smcp::Err;
using smcp::Heartbeat;
using smcp::Session;
using smcp::Node;
template <uint8_t N>
using SessionBank = smcp::SessionBank<N>;
using smcp::proto::Reply;
using smcp::proto::RequestJob;
using smcp::proto::LinkHb;
using Proto = comm::IProto<Frame>;
using St = comm::Job::Status;

struct Root : comm::ProtoContainer<Frame> {};

/** Ловит onLink. На Down база закрывает слот. */
struct Watch : Session {
    using Session::Session;
    LinkHb::Status link = LinkHb::Status::Down;

protected:
    void onLink(smcp::proto::LinkStatus status) noexcept override
    {
        Session::onLink(status);
        link = status;
    }
};

Frame makeAck(addr_t src, uint8_t txn)
{
    Frame p{};
    p.src = src;
    p.txn = txn;
    Ack{}.pack(p);
    return p;
}

Frame makeNack(addr_t src, uint8_t txn, Err err)
{
    Frame p{};
    p.src = src;
    p.txn = txn;
    Nack n{};
    n.error = err;
    n.pack(p);
    return p;
}

[[nodiscard]] Heartbeat::Msg msgOf(const Frame& pkt)
{
    Heartbeat hb{};
    if (!hb.unpack(pkt)) {
        return Heartbeat::Msg::KeepAlive;
    }
    return hb.msg;
}

/** Привязать peer, не стартуя мастерский LinkHb. */
void bind(Session& s, addr_t peer)
{
    static_cast<comm::Session<Frame>&>(s).open(peer);
}

bool pumpTx(Session& sess, Frame& out, MISC::time_t now)
{
    Proto* owner = nullptr;
    if (!sess.peekTx(owner, out, now) || owner == nullptr) {
        return false;
    }
    owner->onTxDone(out, true);
    return true;
}

void test_reply_ack()
{
    std::puts("CASE Reply: submit → TX → Ack → Ok");
    Root root;
    Session sess(root);
    const addr_t peer{2};
    sess.open(peer);
    Reply reply(sess);

    RequestJob job{};
    job.req.id = MsgId::Get;
    CHECK(reply.submit(job));
    CHECK(job.txn != 0u);
    CHECK(job.status() == St::Queued);

    Frame tx{};
    CHECK(pumpTx(sess, tx, 0u));
    CHECK(tx.dst == peer && tx.txn == job.txn);
    CHECK(tx.id == MsgId::Get);
    CHECK(job.status() == St::Active);
    CHECK(reply.isWaiting());

    CHECK(sess.onPacket(makeAck(peer, job.txn), 10u));
    CHECK(job.status() == St::Ok);
    CHECK(!reply.isWaiting());
}

void test_reply_nack()
{
    std::puts("CASE Reply: Nack → Failed");
    Root root;
    Session sess(root);
    sess.open(addr_t{3});
    Reply reply(sess);

    RequestJob job{};
    job.req.id = MsgId::Select;
    CHECK(reply.submit(job));
    Frame tx{};
    CHECK(pumpTx(sess, tx, 0u));

    CHECK(sess.onPacket(makeNack(addr_t{3}, job.txn, Err::Busy), 5u));
    CHECK(job.status() == St::Failed);
    CHECK(job.nack.error == Err::Busy);
}

void test_reply_window()
{
    std::puts("CASE Reply: окно 1 — второй не уходит пока Active");
    Root root;
    Session sess(root);
    sess.open(addr_t{4});
    Reply reply(sess);

    RequestJob a{};
    RequestJob b{};
    a.req.id = MsgId::Get;
    b.req.id = MsgId::Set;
    CHECK(reply.submit(a));
    CHECK(reply.submit(b));

    Frame tx{};
    Proto* owner = nullptr;
    CHECK(sess.peekTx(owner, tx, 0u) && owner != nullptr);
    CHECK(tx.txn == a.txn);
    // пока _wire — второй peekTx false
    Proto* again = nullptr;
    Frame slot{};
    CHECK(!sess.peekTx(again, slot, 1u));

    owner->onTxDone(tx, true);
    CHECK(a.status() == St::Active);
    // Active держит окно — b ещё Queued, peekTx false
    CHECK(!sess.peekTx(again, slot, 2u));

    CHECK(sess.onPacket(makeAck(addr_t{4}, a.txn), 3u));
    CHECK(a.status() == St::Ok);

    CHECK(pumpTx(sess, tx, 4u));
    CHECK(tx.txn == b.txn && b.status() == St::Active);
    CHECK(sess.onPacket(makeAck(addr_t{4}, b.txn), 5u));
    CHECK(b.status() == St::Ok);
}

void test_reply_early_ack()
{
    std::puts("CASE Reply: Ack до onTxDone (Queued)");
    Root root;
    Session sess(root);
    sess.open(addr_t{5});
    Reply reply(sess);

    RequestJob job{};
    job.req.id = MsgId::Get;
    CHECK(reply.submit(job));

    Frame tx{};
    Proto* owner = nullptr;
    CHECK(sess.peekTx(owner, tx, 0u));
    CHECK(job.status() == St::Queued);

    CHECK(sess.onPacket(makeAck(addr_t{5}, job.txn), 1u));
    CHECK(job.status() == St::Ok);

    owner->onTxDone(tx, true); // late done — нет Active
    CHECK(job.status() == St::Ok);
}

void test_reply_retry_timeout()
{
    std::puts("CASE Reply: таймаут → retry → Timeout");
    Root root;
    Session sess(root);
    sess.open(addr_t{6});
    Reply reply(sess);

    RequestJob job{};
    job.req.id = MsgId::Get;
    CHECK(reply.submit(job));
    // один retry для короткого теста
    job.retries = 1;

    Frame tx{};
    CHECK(pumpTx(sess, tx, 0u));
    CHECK(job.attempts() == 1u);

    reply.tick(Ack::kTimeoutMs);
    CHECK(job.status() == St::Queued);

    CHECK(pumpTx(sess, tx, Ack::kTimeoutMs));
    CHECK(job.attempts() == 2u);

    reply.tick(Ack::kTimeoutMs * 2u);
    CHECK(job.status() == St::Timeout);
}

void test_hb_master_slave()
{
    std::puts("CASE LinkHb: Request → Ack → Up, KeepAlive поднимает ведомого");
    Root rootM;
    Root rootS;
    Watch sessM(rootM);
    Watch sessS(rootS);
    const addr_t idM{1};
    const addr_t idS{2};
    sessM.open(idS);
    bind(sessS, idM);
    CHECK(sessM.link == LinkHb::Status::Connecting);
    CHECK(sessS.link == LinkHb::Status::Down);

    sessM.tick(0u);
    Frame req{};
    CHECK(pumpTx(sessM, req, 0u));
    CHECK(req.id == MsgId::Heartbeat);
    CHECK(msgOf(req) == Heartbeat::Msg::Request);

    req.src = idM;
    CHECK(sessS.onPacket(req, 1u));
    CHECK(sessS.link == LinkHb::Status::Connecting);

    sessS.tick(1u);
    Frame ack{};
    CHECK(pumpTx(sessS, ack, 1u));
    CHECK(msgOf(ack) == Heartbeat::Msg::Ack);

    ack.src = idS;
    CHECK(sessM.onPacket(ack, 2u));
    CHECK(sessM.link == LinkHb::Status::Up);
    CHECK(sessS.link == LinkHb::Status::Connecting);

    sessM.tick(2u);
    const MISC::time_t kaAt = 2u + Heartbeat::kTimeoutMs;
    sessM.tick(kaAt);
    Frame ka{};
    CHECK(pumpTx(sessM, ka, kaAt));
    CHECK(msgOf(ka) == Heartbeat::Msg::KeepAlive);

    ka.src = idM;
    CHECK(sessS.onPacket(ka, kaAt));
    CHECK(sessS.link == LinkHb::Status::Up);
}

void test_hb_lost()
{
    std::puts("CASE LinkHb: LostHb после kMissMax отправок без ответа");
    Root root;
    Watch sess(root);
    sess.open(addr_t{9});

    MISC::time_t now = 0;
    for (uint8_t i = 0; i < Heartbeat::kMissMax; ++i) {
        sess.tick(now);
        Frame tx{};
        CHECK(pumpTx(sess, tx, now));
        CHECK(msgOf(tx) == Heartbeat::Msg::Request);
        sess.tick(now);
        CHECK(sess.link == LinkHb::Status::Connecting);
        now += Heartbeat::kTimeoutMs;
    }
    sess.tick(now);
    CHECK(sess.link == LinkHb::Status::LostHb);
    CHECK(!sess.isOpen());
}

void test_hb_rx_resets_attempts()
{
    std::puts("CASE LinkHb: принятый KeepAlive обнуляет попытки");
    Root root;
    Watch sess(root);
    sess.open(addr_t{9});

    MISC::time_t now = 0;
    sess.tick(now);
    Frame tx{};
    CHECK(pumpTx(sess, tx, now));
    sess.tick(now);
    now += Heartbeat::kTimeoutMs;
    sess.tick(now);
    CHECK(pumpTx(sess, tx, now));
    sess.tick(now);
    CHECK(sess.link == LinkHb::Status::Connecting);

    Frame ka{};
    ka.src = addr_t{9};
    Heartbeat{Heartbeat::Msg::Ack}.pack(ka);
    CHECK(sess.onPacket(ka, now));
    CHECK(sess.link == LinkHb::Status::Up);

    ka = {};
    ka.src = addr_t{9};
    Heartbeat{Heartbeat::Msg::KeepAlive}.pack(ka);
    CHECK(sess.onPacket(ka, now));
    CHECK(!pumpTx(sess, tx, now));

    now += Heartbeat::kTimeoutMs;
    for (uint8_t i = 0; i < Heartbeat::kMissMax; ++i) {
        sess.tick(now);
        CHECK(pumpTx(sess, tx, now));
        CHECK(msgOf(tx) == Heartbeat::Msg::KeepAlive);
        sess.tick(now);
        CHECK(sess.link == LinkHb::Status::Up);
        now += Heartbeat::kTimeoutMs;
    }
    sess.tick(now);
    CHECK(sess.link == LinkHb::Status::LostHb);
    CHECK(!sess.isOpen());
}

struct DeadLink : comm::ILink<Frame> {
    State state() const noexcept override { return State::Closed; }
    std::size_t txSpace() const noexcept override { return 0; }
    std::size_t txBusy() const noexcept override { return 0; }
    bool send(const Frame&, void*) noexcept override { return false; }
    bool pollTx(Frame&, void*&, TxResult&) noexcept override { return false; }
    void abortTx() noexcept override {}
    bool receive(Frame&) noexcept override { return false; }
};

bool pumpNode(Node& node, Frame& out, uint32_t now)
{
    Proto* owner = nullptr;
    if (!node.peekTx(owner, out, now) || owner == nullptr) {
        return false;
    }
    owner->onTxDone(out, true);
    return true;
}

Frame makeAsk(addr_t src, addr_t dst)
{
    Frame p{};
    p.src = src;
    p.dst = dst;
    Heartbeat hb{};
    hb.msg = Heartbeat::Msg::Request;
    hb.pack(p);
    return p;
}

void test_hb_route_admits_slave()
{
    std::puts("CASE LinkHbRoute: Request → сессия сама стартует ведомым, Ack");
    DeadLink link;
    Node node(link);
    node.begin(addr_t{1});
    SessionBank<1> bank(node);

    const Frame ask = makeAsk(addr_t{4}, addr_t{1});
    CHECK(node.onPacket(ask, 1u));
    Session* s = bank.find(addr_t{4});
    CHECK(s != nullptr && s->isOpen());

    node.tick(1u);
    Frame pong{};
    CHECK(pumpNode(node, pong, 1u));
    CHECK(pong.dst == addr_t{4});
    CHECK(msgOf(pong) == Heartbeat::Msg::Ack);
}

void test_hb_route_busy()
{
    std::puts("CASE LinkHbRoute: нет слота → Heartbeat Busy");
    DeadLink link;
    Node node(link);
    node.begin(addr_t{1});
    SessionBank<1> bank(node);

    CHECK(node.onPacket(makeAsk(addr_t{4}, addr_t{1}), 1u));
    node.tick(1u);
    Frame pong{};
    CHECK(pumpNode(node, pong, 1u));

    CHECK(node.onPacket(makeAsk(addr_t{5}, addr_t{1}), 2u));
    CHECK(bank.find(addr_t{5}) == nullptr);
    Frame busy{};
    CHECK(pumpNode(node, busy, 2u));
    CHECK(busy.dst == addr_t{5});
    CHECK(msgOf(busy) == Heartbeat::Msg::Busy);
}

void test_hb_master_busy_stops()
{
    std::puts("CASE LinkHb: Busy в Connecting закрывает слот");
    Root root;
    Watch sess(root);
    sess.open(addr_t{9});
    sess.tick(0u);
    Frame ping{};
    CHECK(pumpTx(sess, ping, 0u));
    CHECK(msgOf(ping) == Heartbeat::Msg::Request);

    Frame busy{};
    busy.src = addr_t{9};
    Heartbeat{Heartbeat::Msg::Busy}.pack(busy);
    CHECK(sess.onPacket(busy, 1u));
    CHECK(sess.link == LinkHb::Status::PeerBusy);
    CHECK(!sess.isOpen());
}

} // namespace

int main()
{
    test_reply_ack();
    test_reply_nack();
    test_reply_window();
    test_reply_early_ack();
    test_reply_retry_timeout();
    test_hb_master_slave();
    test_hb_lost();
    test_hb_rx_resets_attempts();
    test_hb_route_admits_slave();
    test_hb_route_busy();
    test_hb_master_busy_stops();

    std::printf("%s  %d ok, %d fail\n", g_fail == 0 ? "PASS" : "FAIL", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
