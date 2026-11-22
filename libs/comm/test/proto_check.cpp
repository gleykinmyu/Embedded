/**
 * @file proto_check.cpp
 * @brief Unit-тесты comm::ProtoContainer / ProtoBank (список, peekTx слот, demux).
 *
 *   make -C libs/comm/test proto-check
 */

#include <cstdint>
#include <cstdio>

#include "comm/old/proto.hpp"

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

struct Packet {
    uint8_t src = 0;
    uint8_t dst = 0;
    uint8_t id = 0;
};

using IProto = comm::IProto<Packet>;

struct Leaf : comm::Proto<Packet> {
    uint8_t id = 0;
    uint8_t src = 0;
    bool haveTx = false;
    bool inFlight = false;
    bool lastTxOk = false;
    int ticks = 0;
    int aborts = 0;
    int rxHits = 0;
    bool broken = false;

    explicit Leaf(comm::ProtoContainer<Packet>& box, uint8_t id_) noexcept
        : comm::Proto<Packet>(box)
        , id(id_)
    {}

    [[nodiscard]] bool peekTx(Packet& out, MISC::time_t) noexcept override
    {
        if (inFlight || !haveTx) {
            return false;
        }
        out = {};
        out.id = id;
        out.dst = id;
        out.src = src;
        inFlight = true;
        haveTx = false;
        return true;
    }

    void onTxDone(const Packet& f, bool ok) noexcept override
    {
        if (!inFlight || f.id != id) {
            return;
        }
        inFlight = false;
        lastTxOk = ok;
    }

    [[nodiscard]] bool onPacket(const Packet& pkt, MISC::time_t) noexcept override
    {
        if (pkt.id != id) {
            return false;
        }
        ++rxHits;
        return true;
    }

    void tick(MISC::time_t) noexcept override { ++ticks; }
    void abort() noexcept override
    {
        ++aborts;
        haveTx = false;
        inFlight = false;
    }

    [[nodiscard]] bool ok() const noexcept override { return !broken; }
};

struct BankLeaf : comm::Proto<Packet> {
    uint8_t id = 0;
    bool haveTx = false;
    bool inFlight = false;
    int ticks = 0;

    explicit BankLeaf(comm::ProtoContainer<Packet>& box) noexcept
        : comm::Proto<Packet>(box)
    {}

    [[nodiscard]] bool peekTx(Packet& out, MISC::time_t) noexcept override
    {
        if (inFlight || !haveTx) {
            return false;
        }
        out = {};
        out.id = id;
        inFlight = true;
        haveTx = false;
        return true;
    }
    void onTxDone(const Packet&, bool) noexcept override
    {
        if (!inFlight) {
            return;
        }
        inFlight = false;
    }
    [[nodiscard]] bool onPacket(const Packet&, MISC::time_t) noexcept override { return false; }
    void tick(MISC::time_t) noexcept override { ++ticks; }
    void abort() noexcept override
    {
        haveTx = false;
        inFlight = false;
    }
};

struct Root : comm::ProtoContainer<Packet> {
    Leaf a{*this, 1};
    Leaf b{*this, 2};
};

void test_register_ok()
{
    std::puts("CASE ProtoContainer: два ребёнка в списке, ok");
    Root r;
    CHECK(r.ok());
    CHECK(!r.empty());
    CHECK(r.a.id == 1u && r.b.id == 2u);
}

void test_rr_tx()
{
    std::puts("CASE peekTx(owner, …) + onTxDone(frame, ok)");
    Root r;
    Packet slot{};
    r.a.haveTx = true;
    r.b.haveTx = true;

    IProto* owner = nullptr;
    CHECK(r.peekTx(owner, slot, 0u) && owner == &r.a && slot.id == 1u);
    owner->onTxDone(slot, true);
    CHECK(r.a.lastTxOk);

    r.a.haveTx = true;
    CHECK(r.peekTx(owner, slot, 1u) && owner == &r.b && slot.id == 2u);
    owner->onTxDone(slot, false);
    CHECK(!r.b.lastTxOk);

    CHECK(r.peekTx(owner, slot, 2u) && owner == &r.a && slot.id == 1u);
    owner->onTxDone(slot, true);
}

void test_rx_demux()
{
    std::puts("CASE onPacket — первый подходящий");
    Root r;
    Packet pkt{};
    pkt.id = 2;
    CHECK(r.onPacket(pkt, 0u));
    CHECK(r.a.rxHits == 0 && r.b.rxHits == 1);
    pkt.id = 9;
    CHECK(!r.onPacket(pkt, 0u));
}

void test_tick_abort()
{
    std::puts("CASE tick / abort всем детям");
    Root r;
    r.a.haveTx = true;
    r.tick(10u);
    CHECK(r.a.ticks == 1 && r.b.ticks == 1);
    r.abort();
    CHECK(r.a.aborts == 1 && r.b.aborts == 1 && !r.a.haveTx);
}

void test_recursive_ok()
{
    std::puts("CASE ok() рекурсивно");
    Root r;
    CHECK(r.ok());
    r.b.broken = true;
    CHECK(!r.ok());
}

void test_bank()
{
    std::puts("CASE ProtoBank: T[N] в списке контейнера");
    struct Box : comm::ProtoContainer<Packet> {};
    Box box;
    comm::ProtoBank<BankLeaf, 3> bank(box);
    Packet slot{};
    CHECK(!bank.empty() && bank.ok() && bank.size == 3u);
    bank.at(0).haveTx = true;
    bank.at(0).id = 7;
    bank.at(2).haveTx = true;
    bank.at(2).id = 9;

    IProto* owner = nullptr;
    CHECK(bank.peekTx(owner, slot, 0u) && owner == &bank.at(0) && slot.id == 7u);
    owner->onTxDone(slot, true);
    CHECK(bank.peekTx(owner, slot, 1u) && owner == &bank.at(2) && slot.id == 9u);

    bank.tick(0u);
    CHECK(bank.at(0).ticks == 1 && bank.at(1).ticks == 1 && bank.at(2).ticks == 1);
}

void test_tx_fills_src()
{
    std::puts("CASE peekTx: протокол пишет src в слот");
    Root r;
    Packet slot{};
    r.a.src = 0x10;
    r.a.haveTx = true;
    IProto* owner = nullptr;
    CHECK(r.peekTx(owner, slot, 0u) && owner == &r.a && slot.src == 0x10u && slot.id == 1u);
    owner->onTxDone(slot, true);
}

void test_empty_peek()
{
    std::puts("CASE peekTx false — нечего слать");
    Root r;
    Packet slot{};
    IProto* owner = nullptr;
    CHECK(!r.peekTx(owner, slot, 0u) && owner == nullptr);
}

void test_outstanding()
{
    std::puts("CASE outstanding у листа; другой ребёнок может peekTx");
    Root r;
    Packet slot{};
    r.a.haveTx = true;
    r.b.haveTx = true;
    IProto* ownerA = nullptr;
    CHECK(r.peekTx(ownerA, slot, 0u) && ownerA == &r.a && slot.id == 1u);
    Packet slotA = slot;
    IProto* again = nullptr;
    CHECK(!static_cast<IProto&>(r.a).peekTx(again, slot, 1u)); // a ещё inFlight
    IProto* ownerB = nullptr;
    CHECK(r.peekTx(ownerB, slot, 1u) && ownerB == &r.b && slot.id == 2u); // b свободен
    ownerA->onTxDone(slotA, true);
    CHECK(r.a.lastTxOk && !r.a.inFlight);
    ownerB->onTxDone(slot, true);
    CHECK(!r.b.inFlight);
}

struct Mid : comm::ProtoContainer<Packet> {
    Leaf leaf;
    explicit Mid(comm::ProtoContainer<Packet>& outer) noexcept
        : leaf{*this, 7}
    {
        outer.attach(*this);
    }
};

struct Outer : comm::ProtoContainer<Packet> {
    Mid mid{*this};
};

void test_nested_owner()
{
    std::puts("CASE два контейнера: owner — лист");
    Outer o;
    Packet slot{};
    o.mid.leaf.haveTx = true;

    IProto* owner = nullptr;
    CHECK(o.peekTx(owner, slot, 0u) && owner == &o.mid.leaf && slot.id == 7u);
    owner->onTxDone(slot, true);
    CHECK(o.mid.leaf.lastTxOk && !o.mid.leaf.inFlight);

    o.mid.leaf.haveTx = true;
    CHECK(o.peekTx(owner, slot, 1u) && owner == &o.mid.leaf);
    owner->onTxDone(slot, false);
    CHECK(!o.mid.leaf.lastTxOk && !o.mid.leaf.inFlight);
}

void test_slot_owner()
{
    std::puts("CASE слот хранит IProto* с peekTx");
    Root r;
    r.a.haveTx = true;
    r.b.haveTx = true;

    IProto* owners[2]{};
    Packet slots[2]{};
    CHECK(r.peekTx(owners[0], slots[0], 0u));
    CHECK(r.peekTx(owners[1], slots[1], 1u));
    CHECK(owners[0] == &r.a && owners[1] == &r.b);

    // done в обратном порядке — по указателю, не по обходу
    owners[1]->onTxDone(slots[1], true);
    CHECK(!r.b.inFlight && r.a.inFlight);
    owners[0]->onTxDone(slots[0], false);
    CHECK(!r.a.inFlight && !r.a.lastTxOk);
}

} // namespace

int main()
{
    std::puts("comm::ProtoContainer / ProtoBank (List)");
    test_register_ok();
    test_rr_tx();
    test_rx_demux();
    test_tick_abort();
    test_recursive_ok();
    test_bank();
    test_tx_fills_src();
    test_empty_peek();
    test_outstanding();
    test_nested_owner();
    test_slot_owner();
    std::printf("  ok=%d fail=%d\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
