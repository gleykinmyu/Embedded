/**
 * @file proto_rest_check.cpp
 * @brief Fault / Tagged / MultiByte / Segment (new_smcp).
 *
 *   make -C host proto-rest-check
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "crc.hpp"
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
using smcp::Fault;
using smcp::Get;
using smcp::Set;
using smcp::SeqData;
using smcp::IBuf;
using smcp::Session;
using smcp::Node;
using smcp::proto::FaultProto;
using smcp::proto::MultiFrame;
using smcp::proto::Tagged;
using smcp::proto::ITaggedSink;
using smcp::proto::Segment;
using smcp::proto::ISegmentSink;
using Proto = comm::IProto<Frame>;

struct Root : comm::ProtoContainer<Frame> {};

bool pumpTx(Session& sess, Frame& out, uint32_t now)
{
    Proto* owner = nullptr;
    if (!sess.peekTx(owner, out, now) || owner == nullptr) {
        return false;
    }
    owner->onTxDone(out, true);
    return true;
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

struct FaultNode : Node {
    using Node::Node;
    int n = 0;
    addr_t src{};
    Fault last{};

protected:
    void onFault(addr_t s, const Fault& f) noexcept override
    {
        ++n;
        src = s;
        last = f;
    }
};

void test_fault()
{
    std::puts("CASE Fault: TX IdConflict + RX onFault");
    DeadLink link;
    FaultNode node(link);
    FaultProto fault(node);

    fault.raiseIdConflict("nodeA");
    Frame tx{};
    Proto* owner = nullptr;
    CHECK(node.peekTx(owner, tx, 0u) && owner == &fault);
    CHECK(tx.dst == addr_t::kBroadcast);
    CHECK(tx.id == MsgId::Fault);
    owner->onTxDone(tx, true);

    Frame rx{};
    rx.src = addr_t{7};
    Fault body{};
    body.reason = Fault::Code::IdConflict;
    body.detail_len = 3;
    body.detail[0] = 'x';
    body.detail[1] = 'y';
    body.detail[2] = 'z';
    body.pack(rx);
    CHECK(node.onPacket(rx, 1u));
    CHECK(node.n == 1 && node.src == addr_t{7});
    CHECK(node.last.reason == Fault::Code::IdConflict && node.last.detail_len == 3u);
}

struct TagSink : ITaggedSink {
    enum class Ev : uint8_t { None, Get, Reply, SetAck, Set };
    Ev ev = Ev::None;
    uint8_t tag = 0;
    uint8_t index = 0;
    uint8_t txn = 0;
    MultiFrame::Error mbErr = MultiFrame::Error::None;
    bool setOk = false;
    Nack nack{};
    uint8_t buf[32]{};
    uint16_t bufCap = 32;
    uint16_t gotLen = 0;
    Err setResult = Err::None;

    void onGet(const Get& body, uint8_t t) noexcept override
    {
        ev = Ev::Get;
        tag = body.tag;
        index = body.index;
        txn = t;
    }

    [[nodiscard]] bool onSetBuffer(uint8_t, uint8_t, uint8_t, uint16_t size,
                                   IBuf& b) noexcept override
    {
        if (size > bufCap) {
            return false;
        }
        b = {buf, bufCap};
        return true;
    }

    [[nodiscard]] Err onSet(uint8_t t, uint8_t i, const IBuf& b) noexcept override
    {
        ev = Ev::Set;
        tag = t;
        index = i;
        gotLen = b.size;
        if (b.size > 0u && b.data != nullptr) {
            std::memcpy(buf, b.data, b.size);
        }
        return setResult;
    }

    void onReply(uint8_t t, uint8_t i, const IBuf& b,
                 MultiFrame::Error err) noexcept override
    {
        ev = Ev::Reply;
        tag = t;
        index = i;
        mbErr = err;
        gotLen = b.size;
        if (b.size > 0u && b.data != nullptr) {
            std::memcpy(buf, b.data, b.size);
        }
    }

    void onSetAck(uint8_t t, uint8_t i, bool ok, const Nack& n) noexcept override
    {
        ev = Ev::SetAck;
        tag = t;
        index = i;
        setOk = ok;
        nack = n;
    }
};

void test_tagged_short_get()
{
    std::puts("CASE Tagged: short Get → SeqData → onReply");
    Root root;
    Session sess(root);
    sess.open(addr_t{2});
    MultiFrame mb(sess);
    TagSink sink;
    Tagged tagged(sess, mb, &sink);

    uint8_t rxbuf[8]{};
    IBuf dst{rxbuf, sizeof(rxbuf)};
    CHECK(tagged.get(0x11, dst, 3) == Tagged::Result::Ok);

    Frame tx{};
    CHECK(pumpTx(sess, tx, 0u));
    CHECK(tx.id == MsgId::Get && tx.txn != 0u);

    const uint8_t raw[3] = {0xAAu, 0xBBu, 0xCCu};
    Frame hdr{};
    hdr.src = addr_t{2};
    hdr.txn = tx.txn;
    SeqData head{};
    head.seq = 0;
    head.first.chunks = 1;
    MISC::Crc32 crc;
    crc.update(raw, sizeof(raw));
    head.first.crc32 = crc;
    head.pack(hdr);
    CHECK(sess.onPacket(hdr, 10u));

    Frame data{};
    data.src = addr_t{2};
    data.txn = tx.txn;
    SeqData body{};
    body.seq = 1;
    body.chunk.len = 3;
    std::memcpy(body.chunk.data, raw, sizeof(raw));
    body.pack(data);
    CHECK(sess.onPacket(data, 11u));
    CHECK(sink.ev == TagSink::Ev::Reply);
    CHECK(sink.mbErr == MultiFrame::Error::None);
    CHECK(sink.gotLen == 3u && sink.buf[0] == 0xAAu && sink.buf[2] == 0xCCu);
    CHECK(sink.tag == 0x11u && sink.index == 3u);
}

void test_tagged_short_set_roundtrip()
{
    std::puts("CASE Tagged: short Set → Ack → onSetAck");
    Root root;
    Session sess(root);
    sess.open(addr_t{4});
    MultiFrame mb(sess);
    TagSink sink;
    Tagged tagged(sess, mb, &sink);

    uint8_t payload[2] = {0x10, 0x20};
    CHECK(tagged.set(0x22, IBuf{payload, 2}, 1) == Tagged::Result::Ok);

    Frame tx{};
    CHECK(pumpTx(sess, tx, 0u));
    CHECK(tx.id == MsgId::Set);

    Frame ack{};
    ack.src = addr_t{4};
    ack.txn = tx.txn;
    Ack{}.pack(ack);
    CHECK(sess.onPacket(ack, 5u));
    CHECK(sink.ev == TagSink::Ev::SetAck && sink.setOk);
}

void test_tagged_inbound_set()
{
    std::puts("CASE Tagged: inbound inline Set → onSet → Ack TX");
    Root root;
    Session sess(root);
    sess.open(addr_t{5});
    MultiFrame mb(sess);
    TagSink sink;
    Tagged tagged(sess, mb, &sink);

    Frame setPkt{};
    setPkt.src = addr_t{5};
    setPkt.txn = 9;
    Set s{};
    s.tag = 0x33;
    s.index = 2;
    s.len = 2;
    s.data[0] = 1;
    s.data[1] = 2;
    s.pack(setPkt);
    CHECK(sess.onPacket(setPkt, 0u));
    CHECK(sink.ev == TagSink::Ev::Set && sink.gotLen == 2u);

    Frame ackTx{};
    CHECK(pumpTx(sess, ackTx, 1u));
    CHECK(ackTx.id == MsgId::Ack && ackTx.txn == 9u);
}

void test_tagged_inbound_get_sendBody()
{
    std::puts("CASE Tagged: inbound Get → sendBody short");
    Root root;
    Session sess(root);
    sess.open(addr_t{6});
    MultiFrame mb(sess);
    TagSink sink;
    Tagged tagged(sess, mb, &sink);

    Frame getPkt{};
    getPkt.src = addr_t{6};
    getPkt.txn = 11;
    Get g{};
    g.tag = 0x44;
    g.index = 0;
    g.pack(getPkt);
    CHECK(sess.onPacket(getPkt, 0u));
    CHECK(sink.ev == TagSink::Ev::Get && sink.txn == 11u);

    uint8_t body[2] = {7, 8};
    CHECK(tagged.sendBody(sink.tag, IBuf{body, 2}, sink.txn, sink.index));

    Frame dataTx{};
    mb.tick(1u);
    CHECK(pumpTx(sess, dataTx, 1u));
    CHECK(dataTx.id == MsgId::SeqData && dataTx.txn == 11u);
}

struct SegLog : ISegmentSink {
    int n = 0;
    MsgId last = MsgId{};
    void onSegment(const Frame& pkt) noexcept override
    {
        ++n;
        last = pkt.id;
    }
};

void test_segment()
{
    std::puts("CASE Segment: demux Select → sink");
    Root root;
    Session sess(root);
    sess.open(addr_t{8});
    SegLog log;
    Segment seg(sess, &log);

    Frame p{};
    p.src = addr_t{8};
    p.id = MsgId::Select;
    p.payload.clear();
    CHECK(sess.onPacket(p, 0u));
    CHECK(log.n == 1 && log.last == MsgId::Select);

    p.id = MsgId::Get;
    CHECK(!sess.onPacket(p, 1u));
    CHECK(log.n == 1);
}

} // namespace

int main()
{
    test_fault();
    test_tagged_short_get();
    test_tagged_short_set_roundtrip();
    test_tagged_inbound_set();
    test_tagged_inbound_get_sendBody();
    test_segment();

    std::printf("%s  %d ok, %d fail\n", g_fail == 0 ? "PASS" : "FAIL", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
