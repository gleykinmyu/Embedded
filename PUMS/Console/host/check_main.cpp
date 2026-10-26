/**
 * @file check_main.cpp
 * @brief In-process матрица vs PROTOCOL.md (transport Node/Session).
 *
 *   make -C host check
 *
 * Покрытие PROTOCOL (транспорт):
 *   Node begin/end/Listen/Ready, SessionFull, IdConflict (+Fault detail name),
 *   чужой Fault IdConflict не рвёт, свой src≠Fault → конфликт, HbLost Ready sticky,
 *   unicast HB bind / broadcast нет, close(None) без Fault, multi-session isolation,
 *   inline|multi Get/Set (kInlineMax), пустой set/get запрещён, один TaggedOps job,
 *   MB half-duplex, Nack Busy (FSM+policy), Body Seq/Crc, MB Timeout/Overflow/Abort,
 *   Set Ack Timeout (TaggedOps, не ReplyWnd), Get→Nack→onReply(Abort).
 * Вне скоупа bench: ReplyWnd class-A (Select…), TxFull/onPackFailed, app Nack≥0x10.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "error_log.hpp"
#include "hub_can.hpp"
#include "sim_node.hpp"
#include "smcp/can_link.hpp"
#include "smcp/message.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

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

struct Agent {
    smcp::host::HubCan can;
    smcp::CanLink link;
    smcp::host::SimNode node;

    Agent(smcp::host::CanHub& hub, smcp::host::ErrorLog& log)
        : can(hub)
        , link(can, 0)
        , node(link, checkClock, &log)
    {
        (void)can.open(0);
    }
};

struct Mesh {
    smcp::host::ErrorLog log;
    smcp::host::CanHub hub;
    std::vector<std::unique_ptr<Agent>> agents;

    Agent& add()
    {
        agents.push_back(std::make_unique<Agent>(hub, log));
        return *agents.back();
    }

    void pump(uint32_t ms, uint32_t step_ms = 10u)
    {
        uint32_t left = ms;
        while (left > 0u) {
            const uint32_t d = left < step_ms ? left : step_ms;
            g_now += d;
            for (auto& a : agents) {
                if (a->node.getStatus() != smcp::Node::Status::Idle) {
                    a->node.update();
                }
            }
            left -= d;
        }
    }

    void ready(Agent& a, uint8_t id)
    {
        a.node.begin(id);
        char name[8]{};
        std::snprintf(name, sizeof(name), "n%02X", static_cast<unsigned>(id));
        a.node.setName(name);
        pump(smcp::msg::Heartbeat::kTimeoutMs + 20u);
    }

    bool openLink(Agent& master, Agent& slave)
    {
        auto* s = master.node.startPeer(slave.node.id());
        if (s == nullptr) {
            return false;
        }
        pump(800u);
        auto* other = slave.node.peer(master.node.id());
        return s->isOpen()
            && other != nullptr
            && other->isOpen();
    }

    void inject(const smcp::Packet& pkt)
    {
        BIF::CAN::Frame frame{};
        if (pkt.pack(frame)) {
            hub.inject(frame);
        }
    }

    void sendAs(Agent& from, const smcp::Packet& pkt)
    {
        BIF::CAN::Frame frame{};
        if (pkt.pack(frame)) {
            hub.deliver(&from.can, frame);
        }
    }
};

void case_begin_invalid()
{
    std::puts("CASE begin invalid / Listen→Ready / end");
    Mesh m;
    Agent& a = m.add();
    a.node.begin(0);
    CHECK(a.node.getStatus() == smcp::Node::Status::Idle);
    a.node.begin(smcp::Packet::kBroadcastId);
    CHECK(a.node.getStatus() == smcp::Node::Status::Idle);
    a.node.begin(1);
    CHECK(a.node.getStatus() == smcp::Node::Status::Listen);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs + 20u);
    CHECK(a.node.isReady());
    a.node.end();
    CHECK(a.node.getStatus() == smcp::Node::Status::Idle);
}

void case_session_open_close()
{
    std::puts("CASE session open / close(None)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    m.log.clear();
    a.node.peer(2)->close(smcp::Node::Fault::None);
    m.pump(50u);
    CHECK(a.node.peer(2) == nullptr
        || a.node.peer(2)->getStatus() == smcp::Session::Status::Idle);
    CHECK(!m.log.has(1, "SessionFault"));
}

void case_start_rejects()
{
    std::puts("CASE startPeer rejects");
    Mesh m;
    Agent& a = m.add();
    CHECK(a.node.startPeer(2) == nullptr);
    m.ready(a, 1);
    CHECK(a.node.startPeer(0) == nullptr);
    auto* s = a.node.startPeer(2);
    CHECK(s != nullptr);
    CHECK(a.node.startPeer(2) == s);
}

void case_set_get_inline_multi()
{
    /* PROTOCOL: kInlineMax=7 — Set DLC=2…8 inline; DLC=1 → DataBegin→Data*. */
    std::puts("CASE set/get inline + multi (kInlineMax boundary)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    auto* s = a.node.peer(2);
    auto* peer = b.node.peer(1);
    uint8_t reply[64]{};
    smcp::IBuf rb{reply, sizeof(reply)};

    uint8_t exact[smcp::msg::kInlineMax]{};
    for (uint8_t i = 0; i < smcp::msg::kInlineMax; ++i) {
        exact[i] = static_cast<uint8_t>(0x10u + i);
    }
    smcp::IBuf ib{exact, smcp::msg::kInlineMax};
    s->clearFlags();
    CHECK(s->set(smcp::host::kTagEcho, ib));
    m.pump(200u);
    CHECK(peer->storeLen() == smcp::msg::kInlineMax);
    s->clearFlags();
    CHECK(s->get(smcp::host::kTagEcho, rb));
    m.pump(300u);
    CHECK(s->lastReplyOk());
    CHECK(s->lastReplyLen() == smcp::msg::kInlineMax);
    CHECK(std::memcmp(s->lastReply(), exact, smcp::msg::kInlineMax) == 0);

    constexpr uint16_t kMulti = static_cast<uint16_t>(smcp::msg::kInlineMax + 1u);
    uint8_t big[16]{};
    for (uint8_t i = 0; i < kMulti; ++i) {
        big[i] = static_cast<uint8_t>(0xA0u + i);
    }
    smcp::IBuf mb{big, kMulti};
    s->clearFlags();
    CHECK(s->set(smcp::host::kTagEcho, mb));
    m.pump(400u);
    CHECK(peer->storeLen() == kMulti);
    s->clearFlags();
    CHECK(s->get(smcp::host::kTagEcho, rb));
    m.pump(400u);
    CHECK(s->lastReplyOk());
    CHECK(s->lastReplyLen() == kMulti);
    CHECK(std::memcmp(s->lastReply(), big, kMulti) == 0);
}

void case_set_get_api_edges()
{
    std::puts("CASE set/get API edges (TaggedOps isBusy)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    uint8_t raw[4] = {1, 2, 3, 4};
    smcp::IBuf ib{raw, 4};
    uint8_t reply[32]{};
    smcp::IBuf rb{reply, sizeof(reply)};
    CHECK(!a.node.sess().set(smcp::host::kTagEcho, ib)); /* !open */
    CHECK(m.openLink(a, b));
    auto* s = a.node.peer(2);
    smcp::IBuf empty{raw, 0};
    CHECK(!s->set(smcp::host::kTagEcho, empty)); /* size=0 */
    CHECK(!s->get(smcp::host::kTagEcho, empty)); /* size=0 */

    CHECK(s->set(smcp::host::kTagEcho, ib));
    CHECK(!s->set(smcp::host::kTagEcho, ib)); /* SetOut* → busy */
    CHECK(!s->get(smcp::host::kTagEcho, rb)); /* один job */
    m.pump(200u);

    /* seed + GetAsk → set/get локально false */
    CHECK(s->set(smcp::host::kTagEcho, ib));
    m.pump(200u);
    s->clearFlags();
    CHECK(s->get(smcp::host::kTagEcho, rb));
    CHECK(!s->get(smcp::host::kTagEcho, rb));
    CHECK(!s->set(smcp::host::kTagEcho, ib));
    m.pump(300u);
    CHECK(s->lastReplyOk());
}

void case_nack_not_supported()
{
    std::puts("CASE Nack NotSupported (empty store / bad tag / policy reject)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    auto* s = a.node.peer(2);

    uint8_t reply[32]{};
    smcp::IBuf rb{reply, sizeof(reply)};
    m.log.clear();
    CHECK(s->get(smcp::host::kTagEcho, rb));
    m.pump(300u);
    CHECK(m.log.has(1, "Nack", "NotSupported"));
    CHECK(!s->lastReplyOk());
    CHECK(s->lastReplyErr() == smcp::MultiByte::Error::Abort);

    m.log.clear();
    uint8_t raw[3] = {1, 2, 3};
    smcp::IBuf ib{raw, 3};
    CHECK(s->set(0x22, ib));
    m.pump(200u);
    CHECK(m.log.has(1, "Nack", "NotSupported"));

    b.node.setRxPolicy(smcp::host::RxPolicy::Reject);
    m.log.clear();
    CHECK(s->set(smcp::host::kTagEcho, ib));
    m.pump(200u);
    CHECK(m.log.has(1, "Nack", "NotSupported"));
}

void case_nack_busy_policy()
{
    std::puts("CASE Nack Busy (RxPolicy)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    b.node.setRxPolicy(smcp::host::RxPolicy::Busy);
    uint8_t reply[8]{};
    smcp::IBuf rb{reply, sizeof(reply)};
    m.log.clear();
    CHECK(a.node.peer(2)->get(smcp::host::kTagEcho, rb));
    m.pump(300u);
    CHECK(m.log.has(1, "Nack", "Busy"));
    CHECK(!a.node.peer(2)->lastReplyOk());
}

void case_fsm_busy_wire()
{
    /* TaggedOps: входящий Set при GetAsk → Nack Busy (не RxPolicy). */
    std::puts("CASE Nack Busy (TaggedOps GetAsk vs inbound Set)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t seed[4] = {1, 2, 3, 4};
    smcp::IBuf ib{seed, 4};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, ib));
    m.pump(200u);

    uint8_t reply[32]{};
    smcp::IBuf rb{reply, sizeof(reply)};
    CHECK(a.node.peer(2)->get(smcp::host::kTagEcho, rb)); /* A: GetAsk + MB Rx */

    uint8_t big[24]{};
    for (uint8_t i = 0; i < 24u; ++i) {
        big[i] = i;
    }
    smcp::IBuf mb{big, 24};
    m.log.clear();
    CHECK(b.node.peer(1)->set(smcp::host::kTagEcho, mb)); /* B Set → A busy */
    m.pump(500u);
    CHECK(m.log.has(2, "Nack", "Busy"));
}

void case_mb_half_duplex()
{
    /* Один MultiByte на сессию: Mode Tx|Rx, не оба. Встречные multi Set → Busy. */
    std::puts("CASE MB half-duplex: dual multi Set → Busy");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t pa[20]{};
    uint8_t pb[20]{};
    for (uint8_t i = 0; i < 20u; ++i) {
        pa[i] = static_cast<uint8_t>(0x10u + i);
        pb[i] = static_cast<uint8_t>(0x80u + i);
    }
    smcp::IBuf ia{pa, 20};
    smcp::IBuf ibuf{pb, 20};
    m.log.clear();
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, ia));
    CHECK(b.node.peer(1)->set(smcp::host::kTagEcho, ibuf));
    m.pump(600u);
    const bool busy =
        m.log.has(1, "Nack", "Busy") || m.log.has(2, "Nack", "Busy");
    CHECK(busy);
}

void case_mb_rx_timeout()
{
    /* GetAsk + MB Rx: peer mute TX → DataBegin::kTimeoutMs → onReply(Timeout). */
    std::puts("CASE MB RX Timeout (GetAsk, mute peer TX)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t seed[4] = {1, 2, 3, 4};
    smcp::IBuf ib{seed, 4};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, ib));
    m.pump(200u);

    b.can.setMute(true, false); /* B слышит Get, тело не отдаёт */
    uint8_t reply[32]{};
    smcp::IBuf rb{reply, sizeof(reply)};
    auto* s = a.node.peer(2);
    s->clearFlags();
    CHECK(s->get(smcp::host::kTagEcho, rb));
    m.pump(smcp::msg::DataBegin::kTimeoutMs * 3u);
    CHECK(!s->lastReplyOk());
    CHECK(s->lastReplyErr() == smcp::MultiByte::Error::Timeout);
}

void case_mb_get_overflow()
{
    /* GetAsk buf меньше DataBegin.size → Overflow. */
    std::puts("CASE MB Overflow (Get buf too small)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t big[16]{};
    for (uint8_t i = 0; i < 16u; ++i) {
        big[i] = static_cast<uint8_t>(0x40u + i);
    }
    smcp::IBuf mb{big, 16};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, mb));
    m.pump(400u);

    uint8_t tiny[8]{};
    smcp::IBuf rb{tiny, sizeof(tiny)};
    auto* s = a.node.peer(2);
    s->clearFlags();
    CHECK(s->get(smcp::host::kTagEcho, rb));
    m.pump(400u);
    CHECK(!s->lastReplyOk());
    CHECK(s->lastReplyErr() == smcp::MultiByte::Error::Overflow);
}

void case_close_mid_set_abort()
{
    /*
     * close(): TaggedOps::abort(notify) до clearTx.
     * SetIn → onSet(Abort); SetOut* → локальный onNack(Body, Abort).
     */
    std::puts("CASE close mid multi-Set → Abort");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t big[32]{};
    smcp::IBuf mb{big, 32};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, mb));
    m.pump(20u); /* A SetOut, B SetIn */

    auto* bs = b.node.peer(1);
    bs->clearFlags();
    bs->close(smcp::Node::Fault::None);
    m.pump(30u);
    CHECK(bs->lastSetErr() == smcp::MultiByte::Error::Abort);
    CHECK(!bs->isOpen());
}

void case_close_setout_nack()
{
    std::puts("CASE close SetOut → local Nack Body Abort");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t big[32]{};
    smcp::IBuf mb{big, 32};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, mb));
    m.pump(20u);
    m.log.clear();
    a.node.peer(2)->close(smcp::Node::Fault::None);
    m.pump(30u);
    CHECK(m.log.has(1, "Nack", "Body"));
}

void case_hblost_mute_tx()
{
    std::puts("CASE HbLost mute tx (slave silent)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    m.log.clear();
    b.can.setMute(true, false);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs * (smcp::msg::Heartbeat::kMissMax + 3u));
    CHECK(m.log.has(1, "SessionFault", "HbLost"));
    CHECK(a.node.isReady());
    b.can.setMute(false, false);
    CHECK(m.openLink(a, b));
}

void case_hblost_mute_rx()
{
    std::puts("CASE HbLost mute rx (master deaf)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    m.log.clear();
    a.can.setMute(false, true);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs * (smcp::msg::Heartbeat::kMissMax + 3u));
    CHECK(m.log.has(1, "SessionFault", "HbLost"));
}

void case_id_conflict_newcomer()
{
    std::puts("CASE IdConflict: newcomer Listen hears incumbent");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    CHECK(a.node.peer(2)->isOpen());

    Agent& dup = m.add();
    dup.node.begin(1);
    dup.node.setName("dup");
    m.pump(smcp::msg::Heartbeat::kTimeoutMs + 40u);
    CHECK(dup.node.getStatus() == smcp::Node::Status::IdConflict);
    CHECK(a.node.isReady());
    CHECK(a.node.peer(2) != nullptr && a.node.peer(2)->isOpen());
    CHECK(b.node.isReady());
}

void case_id_conflict_silent_twins()
{
    std::puts("CASE IdConflict: silent twins, then traffic");
    Mesh m;
    Agent& a = m.add();
    Agent& twin = m.add();
    Agent& c = m.add();
    m.ready(a, 1);
    m.ready(twin, 1);
    m.ready(c, 3);
    CHECK(a.node.isReady());
    CHECK(twin.node.isReady());
    CHECK(a.node.startPeer(3) != nullptr);
    m.pump(400u);
    const bool one_dead =
        a.node.getStatus() == smcp::Node::Status::IdConflict
        || twin.node.getStatus() == smcp::Node::Status::IdConflict;
    CHECK(one_dead);
}

void case_fault_does_not_kill_incumbent()
{
    std::puts("CASE inject Fault IdConflict: incumbent + session live");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    smcp::msg::Fault body{};
    body.error = smcp::msg::Fault::kIdConflict;
    body.setDetailStr("ghost");
    smcp::Message msg{};
    CHECK(body.pack(msg));
    smcp::Packet pkt{};
    pkt.src_id = 1;
    pkt.dst_id = smcp::Packet::kBroadcastId;
    pkt.msg = msg;
    m.inject(pkt);
    m.pump(80u);
    CHECK(a.node.isReady());
    CHECK(a.node.peer(2) != nullptr && a.node.peer(2)->isOpen());
}

void case_id_conflict_clear_retry()
{
    std::puts("CASE IdConflict clear + begin new id");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    Agent& dup = m.add();
    dup.node.begin(1);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs + 40u);
    CHECK(dup.node.getStatus() == smcp::Node::Status::IdConflict);
    dup.node.begin(4);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs + 20u);
    CHECK(dup.node.isReady());
    CHECK(dup.node.id() == 4u);
}

void case_session_full()
{
    std::puts("CASE SessionFull");
    Mesh m;
    Agent& a = m.add();
    m.ready(a, 1);
    m.log.clear();
    for (uint8_t i = 0; i < smcp::host::SimNode::kMaxSessions; ++i) {
        CHECK(a.node.startPeer(static_cast<uint8_t>(0x20u + i)) != nullptr);
    }
    CHECK(a.node.startPeer(0x80) == nullptr);
    CHECK(m.log.has(1, "SessionFull"));
}

void case_ack_timeout()
{
    /* PROTOCOL: Set-клиент Ack-timeout — TaggedOps (не ReplyWnd); Nack::kTimeout. */
    std::puts("CASE Set Ack timeout (TaggedOps)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    b.can.setMute(true, false);
    uint8_t raw[3] = {9, 8, 7};
    smcp::IBuf ib{raw, 3};
    m.log.clear();
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, ib));
    m.pump(static_cast<uint32_t>(smcp::msg::Ack::kTimeoutMs)
        * (smcp::msg::Ack::kRetry + 2u));
    CHECK(m.log.has(1, "Nack", "Timeout"));
}

void case_awaiting_hblost()
{
    /* PROTOCOL: Awaiting + T → close(HbLost); Node Status остаётся Ready. */
    std::puts("CASE Awaiting HbLost (no slave)");
    Mesh m;
    Agent& a = m.add();
    m.ready(a, 1);
    m.log.clear();
    CHECK(a.node.startPeer(0x50) != nullptr);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs * (smcp::msg::Heartbeat::kMissMax + 3u));
    CHECK(m.log.has(1, "SessionFault", "HbLost"));
    CHECK(a.node.isReady());
}

void case_body_bad_seq()
{
    std::puts("CASE Nack Body (bad Data seq)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t big[16]{};
    smcp::IBuf mb{big, 16};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, mb));
    m.pump(30u); /* Set opener + DataBegin на шине */

    smcp::msg::Data chunk{};
    chunk.seq = 9;
    chunk.len = 3;
    chunk.data[0] = 1;
    chunk.data[1] = 2;
    chunk.data[2] = 3;
    smcp::Message msg{};
    CHECK(chunk.pack(msg));
    smcp::Packet pkt{};
    pkt.src_id = 1;
    pkt.dst_id = 2;
    pkt.txn_id = a.node.peer(2)->txnId();
    pkt.msg = msg;
    m.log.clear();
    m.sendAs(a, pkt);
    m.pump(80u);
    CHECK(m.log.has(1, "Nack", "Body"));
    CHECK(m.log.has(1, "Nack", "Seq"));
}

void case_body_bad_crc()
{
    /* PROTOCOL: конец multi — CRC у RX; fail → onSet + Nack(Body, Crc). */
    std::puts("CASE Nack Body (bad CRC)");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    uint8_t payload[8]{};
    for (uint8_t i = 0; i < 8u; ++i) {
        payload[i] = static_cast<uint8_t>(i + 1u);
    }
    smcp::IBuf mb{payload, 8};
    CHECK(a.node.peer(2)->set(smcp::host::kTagEcho, mb));
    m.pump(10u); /* Set opener → B SetIn */
    a.can.setMute(true, false); /* глушим настоящий DataBegin/Data */

    const uint8_t txn = a.node.peer(2)->txnId();
    smcp::Message begin_msg{};
    CHECK(smcp::msg::DataBegin::make(smcp::host::kTagEcho, 8, 0xDEADBEEFu)
              .pack(begin_msg));
    smcp::Packet begin_pkt{};
    begin_pkt.src_id = 1;
    begin_pkt.dst_id = 2;
    begin_pkt.txn_id = txn;
    begin_pkt.msg = begin_msg;
    m.sendAs(a, begin_pkt);

    smcp::Message d0{};
    CHECK(smcp::msg::Data::make(0, payload, 7).pack(d0));
    smcp::Packet p0{};
    p0.src_id = 1;
    p0.dst_id = 2;
    p0.txn_id = txn;
    p0.msg = d0;
    m.sendAs(a, p0);

    smcp::Message d1{};
    CHECK(smcp::msg::Data::make(1, payload + 7, 1).pack(d1));
    smcp::Packet p1{};
    p1.src_id = 1;
    p1.dst_id = 2;
    p1.txn_id = txn;
    p1.msg = d1;
    m.log.clear();
    m.sendAs(a, p1);
    m.pump(80u);
    CHECK(m.log.has(1, "Nack", "Body"));
    CHECK(m.log.has(1, "Nack", "Crc"));
}

void case_own_src_id_conflict()
{
    /* PROTOCOL: любой PDU с src==я (кроме Fault IdConflict) → enterIdConflict. */
    std::puts("CASE IdConflict: own-src Heartbeat");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));

    smcp::Message hb{};
    hb.id = smcp::msg::Heartbeat::kId;
    smcp::Packet pkt{};
    pkt.src_id = 1;
    pkt.dst_id = 2;
    pkt.msg = hb;
    m.inject(pkt);
    m.pump(40u);
    CHECK(a.node.getStatus() == smcp::Node::Status::IdConflict);
}

void case_ghost_hb_bind()
{
    std::puts("CASE unicast HB binds slave; broadcast HB does not");
    Mesh m;
    Agent& a = m.add();
    m.ready(a, 1);

    smcp::Message hb{};
    hb.id = smcp::msg::Heartbeat::kId;
    smcp::Packet bc{};
    bc.src_id = 0x40;
    bc.dst_id = smcp::Packet::kBroadcastId;
    bc.msg = hb;
    m.inject(bc);
    m.pump(40u);
    CHECK(a.node.sessionByPeer(0x40) == nullptr
        || a.node.sessionByPeer(0x40)->getStatus() == smcp::Session::Status::Idle);

    smcp::Packet uni{};
    uni.src_id = 0x41;
    uni.dst_id = 1;
    uni.msg = hb;
    m.inject(uni);
    m.pump(40u);
    auto* s = a.node.sessionByPeer(0x41);
    CHECK(s != nullptr);
    CHECK(s->getStatus() != smcp::Session::Status::Idle);
}

void case_three_node_mesh()
{
    std::puts("CASE three-node: A↔B and A↔C stay independent");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    Agent& c = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    m.ready(c, 3);
    CHECK(m.openLink(a, b));
    CHECK(m.openLink(a, c));
    b.can.setMute(true, false);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs * (smcp::msg::Heartbeat::kMissMax + 3u));
    CHECK(m.log.has(1, "SessionFault", "peer=0x02"));
    CHECK(a.node.peer(3) != nullptr && a.node.peer(3)->isOpen());
    CHECK(c.node.isReady());
}

void case_begin_resets_sessions()
{
    std::puts("CASE begin() closes live sessions");
    Mesh m;
    Agent& a = m.add();
    Agent& b = m.add();
    m.ready(a, 1);
    m.ready(b, 2);
    CHECK(m.openLink(a, b));
    a.node.begin(5);
    m.pump(smcp::msg::Heartbeat::kTimeoutMs + 20u);
    CHECK(a.node.isReady());
    CHECK(a.node.id() == 5u);
    CHECK(a.node.sessionByPeer(2) == nullptr
        || a.node.sessionByPeer(2)->getStatus() == smcp::Session::Status::Idle);
}

} // namespace

int main()
{
    std::puts("SMCP check — in-process error matrix");
    case_begin_invalid();
    case_session_open_close();
    case_start_rejects();
    case_set_get_inline_multi();
    case_set_get_api_edges();
    case_nack_not_supported();
    case_nack_busy_policy();
    case_fsm_busy_wire();
    case_mb_half_duplex();
    case_mb_rx_timeout();
    case_mb_get_overflow();
    case_close_mid_set_abort();
    case_close_setout_nack();
    case_hblost_mute_tx();
    case_hblost_mute_rx();
    case_id_conflict_newcomer();
    case_id_conflict_silent_twins();
    case_fault_does_not_kill_incumbent();
    case_id_conflict_clear_retry();
    case_session_full();
    case_ack_timeout();
    case_awaiting_hblost();
    case_body_bad_seq();
    case_body_bad_crc();
    case_own_src_id_conflict();
    case_ghost_hb_bind();
    case_three_node_mesh();
    case_begin_resets_sessions();

    std::printf("\n%d ok, %d fail\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
