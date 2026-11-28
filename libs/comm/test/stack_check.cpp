/**
 * Хостовая проверка TxJob, Transport, Protocol и тестового стека.
 */

#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

#include "bitsery/custom/extensions.hpp"
#include "comm/job_net.hpp"
#include "comm/protocol.hpp"
#include "comm/stack.hpp"
#include "mock_transport.hpp"

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAIL " << what << '\n';
        ++g_fail;
    }
}

struct BitParams {
    uint8_t prio = 5;
};

struct UpParams {
    uint8_t tag = 0x13;
    uint8_t prio = 5;
};

struct BitPdu {
    uint8_t code = 0xA5;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<8>(s, code);
    }
};

struct BadPdu {
    uint8_t v = 4;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<2>(s, v);
    }
};

struct BigPdu {
    uint8_t data[20]{};
    template <typename S>
    void serialize(S& s)
    {
        s.container1b(data);
    }
};

struct BitBaseHdr {
    uint8_t prio = 0;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<3>(s, prio);
    }
};

struct BitUpHdr {
    uint8_t tag = 0;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<5>(s, tag);
    }
};

struct BitRx {
    uint8_t prio = 0;
};

template <typename Lower>
struct BitUpper;

template <typename Lower>
struct BitBase : comm::Protocol<Lower> {
    explicit BitBase(Lower& lower) noexcept
        : comm::Protocol<Lower>(lower)
    {}

    BitUpper<Lower>* next = nullptr;
    comm::TxJob* spawn = nullptr;
    comm::TxJob* watch = nullptr;
    int* watchDone = nullptr;
    int sends = 0;
    int rx = 0;
    bool bad = false;
    bool sawDoneBeforeRx = false;
    bitsery::custom::Reader* seen = nullptr;

    template <typename Payload>
    bool send(comm::TxJob& job, const Payload& payload, const BitParams& params)
    {
        ++sends;
        BitBaseHdr hdr{};
        hdr.prio = params.prio;
        return this->lower().send(job, comm::Frame{hdr, payload});
    }

    void onReceive(bitsery::custom::Reader& r);
};

template <typename Lower>
struct BitUpper : comm::Protocol<BitBase<Lower>> {
    explicit BitUpper(BitBase<Lower>& lower) noexcept
        : comm::Protocol<BitBase<Lower>>(lower)
    {}

    int sends = 0;
    int rx = 0;
    uint8_t tag = 0;
    uint8_t code = 0;
    uint8_t prio = 0;
    bitsery::custom::Reader* seen = nullptr;

    template <typename Payload>
    bool send(comm::TxJob& job, const Payload& payload, const UpParams& params)
    {
        ++sends;
        BitUpHdr hdr{};
        hdr.tag = params.tag;
        return this->lower().send(job, comm::Frame{hdr, payload}, BitParams{params.prio});
    }

    void onReceive(const BitRx& rxParams, bitsery::custom::Reader& r)
    {
        ++rx;
        seen = &r;
        prio = rxParams.prio;
        BitUpHdr hdr{};
        BitPdu pdu{};
        r.object(hdr);
        r.object(pdu);
        tag = hdr.tag;
        code = pdu.code;
    }
};

template <typename Lower>
void BitBase<Lower>::onReceive(bitsery::custom::Reader& r)
{
    ++rx;
    seen = &r;
    BitBaseHdr hdr{};
    r.object(hdr);
    if (r.adapter().error() != bitsery::ReaderError::NoError) {
        bad = true;
        return;
    }
    if (watch != nullptr && watchDone != nullptr) {
        sawDoneBeforeRx = watch->status() == comm::Job::Status::Ok && *watchDone == 1;
    }
    if (spawn != nullptr) {
        (void)this->requestTx(*spawn);
        spawn = nullptr;
    }
    if (next != nullptr) {
        const BitRx rxParams{hdr.prio};
        next->onReceive(rxParams, r);
    }
}

template <typename Port>
struct Stack {
    Port port;
    BitBase<Port> base;
    BitUpper<Port> upper;

    Stack()
        : base(port)
        , upper(base)
    {
        base.next = &upper;
    }
};

struct AppJob : comm::TxJob {
    static int alive;
    uint8_t tag = 0;
    explicit AppJob(comm::ITxJobOwner& owner) noexcept
        : comm::TxJob(owner)
    {
        ++alive;
    }
    ~AppJob() { --alive; }
};

int AppJob::alive = 0;

enum class Act : uint8_t {
    Send,
    None,
    Twice,
    Cross,
    Bad,
    Big,
    CancelBefore,
    CancelAfter,
    SubmitExtra,
    Retry,
};

template <typename Port>
struct Node : comm::ITxJobOwner {
    Stack<Port>* stack = nullptr;
    AppJob* job = nullptr;
    AppJob* extra = nullptr;
    Stack<Port>* other = nullptr;
    int ready = 0;
    int done = 0;
    int repost = 0;
    bool watchRelease = false;
    bool released = false;
    bool crossOwn = false;
    bool crossOther = true;
    comm::Job::Status last = comm::Job::Status::Idle;
    Act act = Act::Send;
    char log[8]{};
    int nlog = 0;

    void onTxReady(comm::TxJob& current) noexcept override
    {
        ++ready;
        const UpParams params{};
        const BitPdu pdu{};
        switch (act) {
        case Act::Send:
            (void)stack->upper.send(current, pdu, params);
            break;
        case Act::None:
            break;
        case Act::Twice:
            crossOwn = stack->upper.send(current, pdu, params);
            crossOther = stack->upper.send(current, pdu, params);
            break;
        case Act::Cross:
            crossOther = other->upper.send(current, pdu, params);
            crossOwn = stack->upper.send(current, pdu, params);
            break;
        case Act::Bad: {
            const BadPdu bad{};
            crossOwn = stack->upper.send(current, bad, params);
            break;
        }
        case Act::Big: {
            const BigPdu big{};
            crossOwn = stack->upper.send(current, big, params);
            break;
        }
        case Act::CancelBefore:
            crossOther = stack->upper.cancelTx(current);
            break;
        case Act::CancelAfter:
            crossOwn = stack->upper.send(current, pdu, params);
            crossOther = stack->upper.cancelTx(current);
            break;
        case Act::SubmitExtra:
            (void)stack->upper.send(current, pdu, params);
            (void)stack->upper.requestTx(*extra);
            break;
        case Act::Retry:
            crossOther = stack->upper.send(current, pdu, params);
            crossOwn = stack->upper.send(current, pdu, params);
            break;
        }
    }

    void onTxDone(comm::TxJob& current) noexcept override
    {
        ++done;
        last = current.status();
        if (nlog < 8) {
            log[nlog++] = static_cast<AppJob&>(current).tag;
        }
        if (watchRelease && stack != nullptr) {
            released = !stack->port.holds(current);
        }
        if (repost > 0) {
            --repost;
            (void)stack->upper.requestTx(current);
        }
    }
};

template <typename Port>
void arm(Node<Port>& node, Stack<Port>& stack, AppJob& job)
{
    node.stack = &stack;
    node.job = &job;
}

void testRequest()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> node;
    AppJob job{node};
    job.tag = 7;
    arm(node, stack, job);
    const int alive = AppJob::alive;
    check(stack.upper.requestTx(job), "submit");
    check(node.ready == 0 && node.done == 0, "no callback in request");
    check(job.status() == comm::Job::Status::Queued, "queued");
    check(stack.upper.sends == 0 && stack.base.sends == 0, "no frame yet");
    check(!stack.upper.requestTx(job), "busy");
    check(job.status() == comm::Job::Status::Queued && job.tag == 7, "busy unchanged");
    check(AppJob::alive == alive, "one job");
    stack.port.close();
}

void testSuccessAndFifo()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> node;
    AppJob a{node};
    AppJob b{node};
    a.tag = 'A';
    b.tag = 'B';
    arm(node, stack, a);
    arm(node, stack, b);
    check(stack.upper.requestTx(a) && stack.upper.requestTx(b), "fifo submit");
    stack.port.tick(stack.base, 0);
    check(node.ready == 1 && a.status() == comm::Job::Status::Active, "first active");
    check(b.status() == comm::Job::Status::Queued && node.done == 0, "second waits");
    check(stack.upper.sends == 1 && stack.base.sends == 1, "both levels");
    const uint8_t exp[] = {0xB3, 0xA5};
    check(stack.port.bits(0) == 16u && stack.port.size(0) == 2u, "wire size");
    check(std::memcmp(stack.port.data(0), exp, 2) == 0, "wire bytes");
    check(stack.port.slotJob(0) == &a, "slot bound");
    check(stack.port.complete(0, true), "complete");
    check(!stack.port.complete(0, true), "second complete");
    node.watchRelease = true;
    stack.port.tick(stack.base, 1);
    check(node.released, "slot freed before done");
    check(node.done == 1 && a.status() == comm::Job::Status::Ok && node.log[0] == 'A', "A done");
    check(b.status() == comm::Job::Status::Active && node.ready == 2, "B started");
    check(stack.port.slotJob(0) == &b, "slot reused");
    check(stack.port.complete(0, false), "B fail hw");
    stack.port.tick(stack.base, 2);
    check(node.done == 2 && b.status() == comm::Job::Status::Failed && node.log[1] == 'B', "B failed");
}

void testDeclineReuse()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> decline;
    Node<comm::MockTransport<1>> send;
    decline.act = Act::None;
    AppJob a{decline};
    AppJob b{send};
    a.tag = 'A';
    b.tag = 'B';
    arm(decline, stack, a);
    arm(send, stack, b);
    (void)stack.upper.requestTx(a);
    (void)stack.upper.requestTx(b);
    stack.port.tick(stack.base, 0);
    check(a.status() == comm::Job::Status::Failed && decline.done == 1, "decline");
    check(b.status() == comm::Job::Status::Queued && send.ready == 0, "budget spent");
    stack.port.tick(stack.base, 1);
    check(b.status() == comm::Job::Status::Active && send.ready == 1, "next tick");
    stack.port.close();
}

void testSendFaults()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> node;
    AppJob job{node};
    arm(node, stack, job);

    node.act = Act::Twice;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 0);
    check(node.crossOwn && !node.crossOther, "second send rejected");
    check(stack.port.size(0) == 2u && job.status() == comm::Job::Status::Active, "first send stands");
    (void)stack.port.complete(0, true);
    stack.port.tick(stack.base, 1);

    node.act = Act::Bad;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 2);
    check(!node.crossOwn && job.status() == comm::Job::Status::Failed, "bad value");
    check(stack.port.slotJob(0) == nullptr, "failed transmit unbound");

    node.act = Act::Big;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 3);
    check(!node.crossOwn && job.status() == comm::Job::Status::Failed, "oversize");

    stack.port.refuseNext();
    node.act = Act::Send;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 4);
    check(job.status() == comm::Job::Status::Failed && node.last == comm::Job::Status::Failed, "driver refuse");

    node.act = Act::Retry;
    stack.port.refuseNext();
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 5);
    check(!node.crossOther && node.crossOwn, "retry after refuse");
    check(stack.port.size(0) == 2u && job.status() == comm::Job::Status::Active, "retry accepted");
    stack.port.close();
}

void testCrossContext()
{
    Stack<comm::MockTransport<1>> a;
    Stack<comm::MockTransport<1>> b;
    Node<comm::MockTransport<1>> node;
    node.act = Act::Cross;
    node.other = &b;
    AppJob job{node};
    arm(node, a, job);
    (void)a.upper.requestTx(job);
    a.port.tick(a.base, 0);
    check(!node.crossOther && node.crossOwn, "foreign context");
    check(a.port.size(0) == 2u && b.port.size(0) == 0u, "frame on owner port");
    a.port.close();
}

void testCancelAndClose()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> node;
    AppJob job{node};
    arm(node, stack, job);
    (void)stack.upper.requestTx(job);
    check(stack.upper.cancelTx(job), "cancel queued");
    check(node.done == 1 && job.status() == comm::Job::Status::Aborted && node.ready == 0, "queued abort");
    check(!stack.upper.cancelTx(job), "cancel idle");

    node.act = Act::CancelBefore;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 0);
    check(node.ready == 1 && !node.crossOther && node.done == 2, "cancel inside ready");
    check(job.status() == comm::Job::Status::Failed, "no send is failed");

    node.act = Act::CancelAfter;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 1);
    check(node.crossOwn && !node.crossOther && job.status() == comm::Job::Status::Active, "active not cancelled");
    check(node.done == 2, "still in flight");
    (void)stack.port.complete(0, true);
    stack.port.tick(stack.base, 2);
    check(job.status() == comm::Job::Status::Ok && node.done == 3, "hw result stands");

    node.act = Act::Send;
    AppJob queued{node};
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 3);
    (void)stack.upper.requestTx(queued);
    check(!stack.upper.cancelTx(job), "cancel inflight");
    check(job.status() == comm::Job::Status::Active && node.done == 3, "inflight not delivered");
    stack.port.close();
    check(job.status() == comm::Job::Status::Failed, "close reports driver");
    check(queued.status() == comm::Job::Status::Aborted, "close queued");
    check(node.done == 5, "close callbacks");
}

void testRepostAndExtra()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> node;
    AppJob job{node};
    AppJob extra{node};
    extra.tag = 'E';
    arm(node, stack, job);
    node.repost = 1;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 0);
    (void)stack.port.complete(0, true);
    stack.port.tick(stack.base, 1);
    check(node.done == 1 && node.ready == 2 && job.status() == comm::Job::Status::Active, "repost same tick");
    (void)stack.port.complete(0, true);
    stack.port.tick(stack.base, 2);

    node.act = Act::SubmitExtra;
    node.extra = &extra;
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 4);
    check(extra.status() == comm::Job::Status::Queued, "spawn waits a tick");
    (void)stack.port.complete(0, true);
    stack.port.tick(stack.base, 5);
    check(extra.status() == comm::Job::Status::Active, "spawn next tick");
    stack.port.close();
}

void testTimeoutAndReenter()
{
    Stack<comm::MockTransport<1>> stack;
    stack.port.setTimeout(10);
    Node<comm::MockTransport<1>> node;
    AppJob a{node};
    AppJob b{node};
    b.tag = 'B';
    arm(node, stack, a);
    (void)stack.upper.requestTx(a);
    stack.port.tick(stack.base, 0);
    (void)stack.upper.requestTx(b);
    stack.port.tick(stack.base, 10);
    check(a.status() == comm::Job::Status::Timeout && node.done == 1, "physical timeout");
    check(b.status() == comm::Job::Status::Active, "grant after timeout");
    stack.port.close();

    struct ReNode : comm::ITxJobOwner {
        Stack<comm::MockTransport<1>>* stack = nullptr;
        int done = 0;
        void onTxReady(comm::TxJob& job) noexcept override
        {
            const UpParams params{};
            const BitPdu pdu{};
            (void)stack->upper.send(job, pdu, params);
        }
        void onTxDone(comm::TxJob&) noexcept override
        {
            ++done;
            stack->port.tick(stack->base, 0);
            stack->port.close();
        }
    };
    comm::transportCloseFaults() = 0;
    Stack<comm::MockTransport<1>> again;
    ReNode re;
    re.stack = &again;
    AppJob job{re};
    (void)again.upper.requestTx(job);
    again.port.tick(again.base, 0);
    (void)again.port.complete(0, true);
    again.port.tick(again.base, 1);
    check(re.done == 1 && again.port.isOpen(), "nested close ignored");
    check(comm::transportCloseFaults() == 1u, "close fault");
}

void testRx()
{
    Stack<comm::MockTransport<1>> stack;
    Node<comm::MockTransport<1>> node;
    AppJob job{node};
    arm(node, stack, job);
    (void)stack.upper.requestTx(job);
    stack.port.tick(stack.base, 0);
    (void)stack.port.complete(0, true);
    const uint8_t frame[] = {0xB3, 0xA5};
    check(stack.port.inject(frame, sizeof frame), "inject");
    stack.base.watch = &job;
    stack.base.watchDone = &node.done;
    stack.port.tick(stack.base, 1);
    check(stack.base.sawDoneBeforeRx, "done before rx");
    check(node.done == 1 && job.status() == comm::Job::Status::Ok, "done after rx");
    check(stack.base.seen == stack.upper.seen, "one reader");
    check(stack.upper.prio == 5 && stack.upper.tag == 0x13 && stack.upper.code == 0xA5, "rx fields");
    check(!stack.base.bad, "rx ok");

    const uint8_t more[] = {0x11, 0x22, 0x33, 0x44};
    for (uint8_t i = 0; i < 4u; ++i) {
        check(stack.port.inject(more + i, 1), "inject stream");
    }
    AppJob flow{node};
    (void)stack.upper.requestTx(flow);
    const int rxBefore = stack.base.rx;
    stack.port.tick(stack.base, 2);
    check(stack.base.rx == rxBefore + 2, "rx cap");
    check(stack.port.rxPending() == 2u, "rx left");
    check(flow.status() == comm::Job::Status::Active, "tx not starved");

    check(stack.port.complete(0, true), "free slot");
    stack.base.spawn = &job;
    const uint8_t one[] = {0x00};
    check(stack.port.inject(one, 1), "inject spawn");
    stack.port.tick(stack.base, 3);
    check(job.status() == comm::Job::Status::Active, "spawn same tick");
    stack.port.close();

    Stack<comm::MockTransport<1>> broken;
    check(broken.port.inject(nullptr, 0) == false, "empty inject rejected");
    const uint8_t nothing[] = {0};
    check(broken.port.inject(nothing, 0), "zero length");
    broken.port.tick(broken.base, 0);
    check(broken.base.bad && broken.upper.rx == 0, "bad header");
}

struct AltByte {
    uint8_t value = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.value1b(value);
    }
};

void testAltAndSlots()
{
    struct One : comm::Protocol<comm::MockTransport<1>> {
        explicit One(comm::MockTransport<1>& port) noexcept
            : comm::Protocol<comm::MockTransport<1>>(port)
        {}
        AltByte got{};
        bool called = false;
        void onReceive(bitsery::custom::Reader& r)
        {
            called = true;
            r.object(got);
        }
    };
    static_assert(std::is_same<One::lower_type, comm::MockTransport<1>>::value, "same port type");

    comm::MockTransport<1> port;
    One alt{port};
    const uint8_t byte[] = {0x5A};
    check(port.inject(byte, 1), "alt inject");
    port.tick(alt, 0);
    check(alt.called && alt.got.value == 0x5A, "other upper");

    Stack<comm::MockTransport<2>> dual;
    Node<comm::MockTransport<2>> node;
    AppJob a{node};
    AppJob b{node};
    a.tag = 'A';
    b.tag = 'B';
    arm(node, dual, a);
    (void)dual.upper.requestTx(a);
    (void)dual.upper.requestTx(b);
    dual.port.tick(dual.base, 0);
    check(a.status() == comm::Job::Status::Active && b.status() == comm::Job::Status::Active, "two slots");
    check(dual.port.slotJob(0) == &a && dual.port.slotJob(1) == &b, "both bound");
    check(dual.port.complete(1, true), "finish B first");
    node.watchRelease = true;
    dual.port.tick(dual.base, 1);
    check(node.released && dual.port.slotJob(1) == nullptr, "B released");
    check(dual.port.slotJob(0) == &a, "A still bound");
    check(node.done == 1 && node.log[0] == 'B' && a.status() == comm::Job::Status::Active, "out of order");
    (void)dual.port.complete(0, true);
    dual.port.tick(dual.base, 2);
    check(node.log[1] == 'A' && node.done == 2, "A later");
}

void testLogicalList()
{
    struct Logical : comm::JobNet {
        comm::TxJob tx;
        explicit Logical(comm::ITxJobOwner& owner) noexcept
            : tx(owner)
        {}
    };
    struct NetOwner : comm::IJobOwner {
        int done = 0;
        void onDone(comm::Job&) noexcept override { ++done; }
    };
    struct TxOwner : comm::ITxJobOwner {
        int ready = 0;
        void onTxReady(comm::TxJob&) noexcept override { ++ready; }
        void onTxDone(comm::TxJob&) noexcept override {}
    };
    Stack<comm::MockTransport<1>> stack;
    TxOwner txOwner;
    comm::JobListNet<Logical> logical{50};
    Logical item{txOwner};
    NetOwner net;
    item.owner = &net;
    item.txn = 3;
    check(logical.submit(item), "logical submit");
    check(stack.upper.requestTx(item.tx), "tx submit");
    check(logical.cancel(item), "cancel logical");
    check(net.done == 1 && item.tx.status() == comm::Job::Status::Queued, "lists independent");
    check(txOwner.ready == 0, "tx not started");
    stack.port.close();
}

void testLifetime()
{
    comm::txLifetimeFaults() = 0;
    {
        comm::MockTransport<1> port;
        Node<comm::MockTransport<1>> node;
        {
            AppJob job{node};
            check(port.requestTx(job), "queue for drop");
        }
        check(comm::txLifetimeFaults() == 0u, "queued drop");
    }

    comm::txLifetimeFaults() = 0;
    alignas(comm::MockTransport<1>) unsigned char rawPort[sizeof(comm::MockTransport<1>)];
    alignas(AppJob) unsigned char rawJob[sizeof(AppJob)];
    auto* port = new (rawPort) comm::MockTransport<1>;
    BitBase<comm::MockTransport<1>> base(*port);
    BitUpper<comm::MockTransport<1>> upper(base);
    base.next = &upper;
    struct Local : comm::ITxJobOwner {
        BitUpper<comm::MockTransport<1>>* upper = nullptr;
        void onTxReady(comm::TxJob& job) noexcept override
        {
            const UpParams params{};
            const BitPdu pdu{};
            (void)upper->send(job, pdu, params);
        }
        void onTxDone(comm::TxJob&) noexcept override {}
    };
    Local local;
    local.upper = &upper;
    auto* job = new (rawJob) AppJob{local};
    check(upper.requestTx(*job), "arm lifetime");
    port->tick(base, 0);
    check(job->status() == comm::Job::Status::Active, "held");
    job->~AppJob();
    check(comm::txLifetimeFaults() == 1u, "inflight drop");
}

struct WordBaseP {
    uint16_t id = 0x1234;
};
struct WordUpP {
    uint16_t seq = 0xABCD;
    uint16_t id = 0x1234;
};
struct WordBaseH {
    uint16_t id = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.value2b(id);
    }
};
struct WordUpH {
    uint16_t seq = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.value2b(seq);
    }
};
struct WordPdu {
    uint32_t word = 0xA1B2C3D4u;
    template <typename S>
    void serialize(S& s)
    {
        s.value4b(word);
    }
};

template <typename Lower>
struct WordBase : comm::Protocol<Lower> {
    explicit WordBase(Lower& lower) noexcept
        : comm::Protocol<Lower>(lower)
    {}
    template <typename Payload>
    bool send(comm::TxJob& job, const Payload& payload, const WordBaseP& params)
    {
        WordBaseH hdr{};
        hdr.id = params.id;
        return this->lower().send(job, comm::Frame{hdr, payload});
    }
    void onReceive(bitsery::custom::Reader&) {}
};

template <typename Lower>
struct WordUpper : comm::Protocol<WordBase<Lower>> {
    explicit WordUpper(WordBase<Lower>& lower) noexcept
        : comm::Protocol<WordBase<Lower>>(lower)
    {}
    template <typename Payload>
    bool send(comm::TxJob& job, const Payload& payload, const WordUpP& params)
    {
        WordUpH hdr{};
        hdr.seq = params.seq;
        return this->lower().send(job, comm::Frame{hdr, payload}, WordBaseP{params.id});
    }
};

void testWordStack()
{
    using Port = comm::MockTransport<1>;
    Port port;
    WordBase<Port> base{port};
    WordUpper<Port> upper{base};
    struct Owner : comm::ITxJobOwner {
        WordUpper<Port>* upper = nullptr;
        void onTxReady(comm::TxJob& job) noexcept override
        {
            const WordPdu pdu{};
            const WordUpP params{};
            (void)upper->send(job, pdu, params);
        }
        void onTxDone(comm::TxJob&) noexcept override {}
    };
    Owner owner;
    owner.upper = &upper;
    AppJob job{owner};
    check(upper.requestTx(job), "word request");
    port.tick(base, 0);
    const uint8_t exp[] = {0x34, 0x12, 0xCD, 0xAB, 0xD4, 0xC3, 0xB2, 0xA1};
    check(port.bits(0) == 64u && port.size(0) == 8u, "word size");
    check(std::memcmp(port.data(0), exp, sizeof exp) == 0, "word bytes");
    port.close();
}

using Port1 = comm::MockTransport<1>;
using SampleFrame = comm::Frame<BitBaseHdr, BitPdu>;

static_assert(sizeof(comm::TxJob) == sizeof(comm::Job), "TxJob has no extra fields");
static_assert(sizeof(comm::TxSlot) == sizeof(comm::TxJob*), "TxSlot is a pointer");
static_assert(!std::is_polymorphic<comm::TxSlot>::value, "TxSlot");
static_assert(!std::is_polymorphic<comm::TxJob>::value, "TxJob");
static_assert(!std::is_polymorphic<Port1>::value, "MockTransport");
static_assert(!std::is_convertible<comm::TxJob*, comm::Job*>::value, "job hidden");
static_assert(!std::is_convertible<comm::ITxJobOwner*, comm::IJobOwner*>::value, "owner hidden");
static_assert(!std::is_polymorphic<SampleFrame>::value, "Frame");
static_assert(!std::is_polymorphic<BitBase<Port1>>::value, "base proto");
static_assert(!std::is_polymorphic<BitUpper<Port1>>::value, "upper proto");
static_assert(!std::is_polymorphic<BitBaseHdr>::value, "header");
static_assert(!std::is_polymorphic<BitPdu>::value, "pdu");
static_assert(std::is_polymorphic<comm::ITxJobOwner>::value, "owner callback");
static_assert(!std::has_virtual_destructor<comm::ITxJobOwner>::value, "owner dtor");
static_assert(!std::has_virtual_destructor<comm::IJobOwner>::value, "job owner dtor");
static_assert(!std::is_base_of<comm::Protocol<Port1>, BitBaseHdr>::value, "header not protocol");

} // namespace

int main()
{
    std::cout << "sizeof host Job=" << sizeof(comm::Job)
              << " TxJob=" << sizeof(comm::TxJob)
              << " TxSlot=" << sizeof(comm::TxSlot)
              << " MockTransport=" << sizeof(comm::MockTransport<1>)
              << " Frame=" << sizeof(SampleFrame) << '\n';
    testRequest();
    testSuccessAndFifo();
    testDeclineReuse();
    testSendFaults();
    testCrossContext();
    testCancelAndClose();
    testRepostAndExtra();
    testTimeoutAndReenter();
    testRx();
    testAltAndSlots();
    testLogicalList();
    testWordStack();
    testLifetime();
    if (g_fail != 0) {
        std::cerr << g_fail << " failed\n";
        return 1;
    }
    std::cout << "OK stack\n";
    return 0;
}
