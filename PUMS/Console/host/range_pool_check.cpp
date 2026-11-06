/**
 * @file range_pool_check.cpp
 * @brief Unit-тесты mem::Range / SlotRange / AllocRange / RangeAllocator.
 *
 *   make -C host pool-check
 */

#include <cstdint>
#include <cstdio>

#include "mem/range_alloc.hpp"
#include "mem/slot.hpp"

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

using Slots = mem::SlotStore16<uint32_t, 16>;
using Pool = mem::RangeAllocator16<uint32_t, 4>;

struct Env {
    Slots slots{};
    Pool a;

    Env() noexcept : slots(), a(slots) {}
};


void test_range_pod()
{
    mem::Range<> d{};
    CHECK(d.empty());
    CHECK(d.first == 0u);
    CHECK(d.count == 0u);
    CHECK(d.end() == 0u);

    d = {2, 3};
    CHECK(!d.empty());
    CHECK(d.end() == 5u);
}

void test_slot_range()
{
    mem::SlotStore16<uint32_t, 8> s;
    *s.at(2) = 20u;
    *s.at(3) = 21u;

    mem::SlotRange<uint32_t> r(s, mem::Range<>{2, 2});
    CHECK(!r.empty());
    CHECK(r.range().first == 2u);
    CHECK(r.at(0) != nullptr && *r.at(0) == 20u);
    CHECK(r.at(1) != nullptr && *r.at(1) == 21u);
    CHECK(r.at(2) == nullptr);
}

void test_empty_pool()
{
    Env e;
    CHECK(e.slots.capacity() == 16u);
    CHECK(e.a.capacity() == 4u);
    CHECK(e.a.freeTop() == 0u);
    CHECK(e.a.freeSlots() == 16u);
    CHECK(e.a.isLayoutOk());
    CHECK(e.a.at(0) != nullptr);
    CHECK(e.a.at(0)->empty());
    CHECK(e.a.at(0)->data() == nullptr);
}

void test_alloc_bump()
{
    Env e;
    CHECK(e.a.at(0)->resize(3));
    CHECK(e.a.at(0)->range().first == 0u);
    CHECK(e.a.at(0)->range().count == 3u);
    CHECK(e.a.freeTop() == 3u);

    CHECK(e.a.at(1)->resize(2));
    CHECK(e.a.at(1)->range().first == 3u);
    CHECK(e.a.at(1)->range().count == 2u);
    CHECK(e.a.freeTop() == 5u);
    CHECK(e.a.isLayoutOk());

    *e.a.at(0)->at(0) = 10u;
    *e.a.at(0)->at(1) = 11u;
    *e.a.at(0)->at(2) = 12u;
    CHECK(*e.a.at(0)->at(0) == 10u);
    CHECK(*e.a.at(0)->at(2) == 12u);
}

void test_shrink_and_clear_hole()
{
    Env e;
    CHECK(e.a.at(0)->resize(4));
    CHECK(e.a.at(1)->resize(4));
    CHECK(e.a.freeTop() == 8u);

    CHECK(e.a.at(0)->resize(2));
    CHECK(e.a.at(0)->range().count == 2u);
    CHECK(e.a.at(0)->range().first == 0u);
    CHECK(e.a.freeTop() == 8u);

    e.a.at(0)->free();
    CHECK(e.a.at(0)->empty());
    CHECK(e.a.at(0)->range().first == 0u);
    CHECK(e.a.freeTop() == 8u);
    CHECK(e.a.isLayoutOk());
}

void test_grow_at_tail()
{
    Env e;
    CHECK(e.a.at(0)->resize(2));
    CHECK(e.a.at(0)->resize(5));
    CHECK(e.a.at(0)->range().first == 0u);
    CHECK(e.a.at(0)->range().count == 5u);
    CHECK(e.a.freeTop() == 5u);
}

void test_grow_relocates()
{
    Env e;
    CHECK(e.a.at(0)->resize(2));
    CHECK(e.a.at(1)->resize(2));
    *e.a.at(0)->at(0) = 100u;
    *e.a.at(0)->at(1) = 101u;

    CHECK(e.a.at(0)->resize(3));
    CHECK(e.a.at(0)->range().first == 4u);
    CHECK(e.a.at(0)->range().count == 3u);
    CHECK(*e.a.at(0)->at(0) == 100u);
    CHECK(*e.a.at(0)->at(1) == 101u);
    CHECK(*e.a.at(0)->at(2) == 0u);
    CHECK(e.a.freeTop() == 7u);
    CHECK(e.a.isLayoutOk());
}

void test_full_and_resize_compact()
{
    Env e;
    CHECK(e.a.at(0)->resize(8));
    CHECK(e.a.at(1)->resize(8));
    CHECK(e.a.freeSlots() == 0u);
    CHECK(!e.a.at(2)->resize(1));

    e.a.at(0)->free();
    CHECK(e.a.at(2)->resize(4));
    CHECK(e.a.at(1)->range().first == 0u);
    CHECK(e.a.at(1)->range().count == 8u);
    CHECK(e.a.at(2)->range().first == 8u);
    CHECK(e.a.at(2)->range().count == 4u);
    CHECK(e.a.freeTop() == 12u);
    CHECK(e.a.isLayoutOk());
}

void test_compact_preserves_values()
{
    Env e;
    CHECK(e.a.at(0)->resize(3));
    CHECK(e.a.at(1)->resize(3));
    CHECK(e.a.at(2)->resize(3));
    *e.a.at(1)->at(0) = 21u;
    *e.a.at(1)->at(1) = 22u;
    *e.a.at(1)->at(2) = 23u;

    e.a.at(0)->free();
    e.a.compact();
    CHECK(e.a.at(0)->empty());
    CHECK(e.a.at(1)->range().first == 0u);
    CHECK(e.a.at(1)->range().count == 3u);
    CHECK(e.a.at(2)->range().first == 3u);
    CHECK(*e.a.at(1)->at(0) == 21u);
    CHECK(*e.a.at(1)->at(1) == 22u);
    CHECK(*e.a.at(1)->at(2) == 23u);
    CHECK(e.a.freeTop() == 6u);
}


void test_alloc_free()
{
    Env e;
    CHECK(e.a.alloc(0) == nullptr);

    auto* a = e.a.alloc(3);
    auto* b = e.a.alloc(2);
    auto* c = e.a.alloc(1);
    auto* d = e.a.alloc(1);
    CHECK(a != nullptr && b != nullptr && c != nullptr && d != nullptr);
    CHECK(!a->isFree() && !b->isFree());
    CHECK(a->range().first == 0u);
    CHECK(b->range().first == 3u);
    CHECK(e.a.alloc(1) == nullptr); // table full

    CHECK(e.a.free(a));
    CHECK(a->isFree());
    CHECK(!e.a.free(nullptr));
    auto* e2 = e.a.alloc(4);
    CHECK(e2 == a);
    CHECK(e2->range().count == 4u);

    CHECK(e.a.alloc(100) == nullptr); // no payload
}

void test_idempotent_resize()
{
    Env e;
    CHECK(e.a.at(0)->resize(3));
    CHECK(e.a.at(0)->resize(3));
    CHECK(e.a.freeTop() == 3u);
    CHECK(e.a.at(0)->resize(0));
    CHECK(e.a.at(0)->empty());
}

} // namespace

int main()
{
    std::printf("mem::RangeAllocator / AllocRange checks\n");
    test_range_pod();
    test_slot_range();
    test_empty_pool();
    test_alloc_bump();
    test_shrink_and_clear_hole();
    test_grow_at_tail();
    test_grow_relocates();
    test_full_and_resize_compact();
    test_compact_preserves_values();
    test_alloc_free();
    test_idempotent_resize();

    std::printf("  ok=%d fail=%d\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
