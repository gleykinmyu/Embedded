/**
 * @file mem_check.cpp
 * @brief Unit-тесты mem::List / ObjRegistry (NoIds / WithIds) / ObjRegStore.
 *
 *   make -C libs/mem/test mem-check
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "mem/obj_registry.hpp"
#include "mem/polyslot.hpp"
#include "mem/list.hpp"

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

// --- List ---

struct Item : mem::ListNode {
    explicit Item(int v) noexcept : value(v) {}
    int value;
};

using List = mem::List<Item>;

int countOf(const List& q)
{
    int n = 0;
    q.forEach([&](const Item&) { ++n; });
    return n;
}

void test_list_fifo()
{
    List q;
    Item a{1}, b{2}, c{3};

    CHECK(q.empty());
    CHECK(countOf(q) == 0);
    CHECK(q.front() == nullptr);
    CHECK(q.popFront() == nullptr);

    CHECK(q.pushBack(a));
    CHECK(q.pushBack(b));
    CHECK(q.pushBack(c));
    CHECK(countOf(q) == 3);
    CHECK(q.front() == &a);
    CHECK(q.back() == &c);
    CHECK(a.linked() && b.linked() && c.linked());

    CHECK(q.popFront() == &a);
    CHECK(!a.linked());
    CHECK(q.popFront() == &b);
    CHECK(q.popFront() == &c);
    CHECK(q.empty());
    CHECK(q.back() == nullptr);
}

static_assert(sizeof(mem::ListNode) == 2u * sizeof(void*), "ListNode: next + prev");

void test_list_single()
{
    List q;
    Item a{1};
    CHECK(q.pushBack(a));     // голова = хвост
    CHECK(a.linked());
    CHECK(countOf(q) == 1);
    CHECK(q.contains(a));
    CHECK(!q.pushBack(a));
    CHECK(!q.pushFront(a));
    CHECK(countOf(q) == 1);

    int visited = 0;
    q.forEach([&](Item&) { ++visited; });
    CHECK(visited == 1);

    CHECK(q.remove(a));
    CHECK(!a.linked());
    CHECK(q.empty() && q.front() == nullptr && q.back() == nullptr);
}

void test_list_double_push()
{
    List q1;
    List q2;
    Item a{1};

    CHECK(q1.pushBack(a));
    CHECK(!q1.pushBack(a));   // уже в списке
    CHECK(!q2.pushBack(a));   // в другом списке
    CHECK(countOf(q1) == 1);
    CHECK(q2.empty());

    CHECK(q1.remove(a));
    CHECK(q2.pushBack(a));    // после снятия — можно
    CHECK(q2.contains(a));
    CHECK(!q1.contains(a));
}

void test_list_remove()
{
    List q;
    Item a{1}, b{2}, c{3}, d{4};
    CHECK(q.pushBack(a));
    CHECK(q.pushBack(b));
    CHECK(q.pushBack(c));

    CHECK(!q.remove(d));      // не в списке

    CHECK(q.remove(b));       // середина
    CHECK(countOf(q) == 2);
    CHECK(q.front() == &a);
    CHECK(q.back() == &c);

    CHECK(q.remove(c));       // хвост
    CHECK(!c.linked());
    CHECK(q.back() == &a);
    CHECK(a.linked());        // новый хвост остался в списке
    CHECK(countOf(q) == 1);
    CHECK(q.pushBack(d));     // хвост обновился корректно
    CHECK(q.back() == &d);
    CHECK(countOf(q) == 2);

    CHECK(q.remove(a));       // голова
    CHECK(q.front() == &d);
    CHECK(q.remove(d));
    CHECK(q.empty());
    CHECK(q.back() == nullptr);
}

void test_list_push_front()
{
    List q;
    Item a{1}, b{2}, c{3};
    CHECK(q.pushFront(a));    // пустой: голова = хвост
    CHECK(q.front() == &a && q.back() == &a);
    CHECK(q.pushBack(b));     // хвост после pushFront в пустой
    CHECK(q.back() == &b);
    CHECK(q.pushFront(c));    // c, a, b
    CHECK(q.front() == &c && q.back() == &b);
    CHECK(q.popFront() == &c);
    CHECK(q.popFront() == &a);
    CHECK(q.popFront() == &b);
    CHECK(q.empty() && q.back() == nullptr);
}

void test_list_foreach()
{
    List q;
    Item a{1}, b{2}, c{3};
    CHECK(q.pushBack(a));
    CHECK(q.pushBack(b));
    CHECK(q.pushBack(c));

    int sum = 0;
    q.forEach([&](Item& it) { sum += it.value; });
    CHECK(sum == 6);

    int visited = 0;
    q.forEach([&](Item&) -> bool { return ++visited == 2; });
    CHECK(visited == 2);

    /* снятие текущего внутри обхода */
    q.forEach([&](Item& it) {
        if (it.value == 2) {
            (void)q.remove(it);
        }
    });
    CHECK(countOf(q) == 2);
    CHECK(!b.linked());
    CHECK(q.front() == &a && q.back() == &c);
}

void test_list_clear()
{
    Item a{1}, b{2};
    {
        List q;
        CHECK(q.pushBack(a));
        CHECK(q.pushBack(b));
        q.clear();
        CHECK(q.empty());
        CHECK(!a.linked() && !b.linked());
        CHECK(q.pushBack(a));
    } /* ~List снимает элементы */
    CHECK(!a.linked());
}

// --- ObjRegistry<T, NoIds> (объект не знает своего места) ---

struct Entry {
    int value = 0;
};

/** Самостоятельная регистрация в ctor. */
struct SelfReg : Entry {
    explicit SelfReg(mem::IRegistrar<Entry>& r) noexcept { r.registerAuto(*this); }
};

/* у подстановки NoIds нет методов WithIds */
template <typename R, typename = void>
struct has_rebind : std::false_type {};
template <typename R>
struct has_rebind<R, std::void_t<decltype(std::declval<R&>().rebind(0, 0))>> : std::true_type {};

void test_noids_basic()
{
    using Reg = mem::ObjRegStore<Entry, 4>;
    static_assert(!has_rebind<Reg>::value, "NoIds: rebind недоступен");

    Reg r;
    CHECK(r.capacity() == 4u && r.count() == 0u && r.isOk());
    CHECK(r.get(0) == nullptr && r.get(7) == nullptr);

    Entry a, b, c;
    CHECK(r.registerAuto(a) == mem::RegStatus::Ok);
    CHECK(r.registerAuto(b) == mem::RegStatus::Ok);
    CHECK(r.registerAuto(c) == mem::RegStatus::Ok);
    CHECK(r.registerAuto(a) == mem::RegStatus::Ok);   // тот же объект — no-op
    CHECK(r.get(0) == &a && r.get(1) == &b && r.get(2) == &c && r.get(3) == nullptr);
    CHECK(r.count() == 3u && r.contains(b) && r.isOk());

    int sum = 0;
    r.forEach([&](Entry& e) { sum += (&e == &b) ? 10 : 1; });
    CHECK(sum == 12);

    int visited = 0;
    r.forEach([&](Entry&) -> bool { return ++visited == 2; });
    CHECK(visited == 2);

    CHECK(r.unregister(b));
    CHECK(r.get(1) == nullptr && !r.contains(b));
    CHECK(!r.unregister(b));                      // уже снят

    r.clear();
    CHECK(r.count() == 0u && r.get(0) == nullptr && r.get(2) == nullptr);
}

void test_noids_selfreg()
{
    mem::ObjRegStore<Entry, 3> r;
    SelfReg a{r}, b{r}, c{r};
    CHECK(r.get(0) == &a && r.get(1) == &b && r.get(2) == &c);
    CHECK(r.isFull() && r.isOk());

    CHECK(r.unregister(b));
    SelfReg d{r};             // дыра 1 переиспользуется
    CHECK(r.get(1) == &d && r.isOk());

    SelfReg e{r};             // нет места → липкий NoFreeSlot
    CHECK(!r.isOk());
    CHECK(r.regStatus() == mem::RegStatus::NoFreeSlot);
    CHECK(!r.contains(e));

    int visited = 0;          // снятие текущего внутри обхода: все занятые пройдены
    r.forEach([&](Entry& it) {
        ++visited;
        (void)r.unregister(it);
    });
    CHECK(visited == 3 && r.count() == 0u);
    CHECK(r.regStatus() == mem::RegStatus::NoFreeSlot);   // липкий — остаётся

    Entry* cells[2]{};        // внешний буфер
    mem::ObjRegistry<Entry> ext(cells, 2);
    SelfReg f{ext};
    CHECK(cells[0] == &f && ext.count() == 1u);
}

// --- ObjRegistry<T, WithIds> (объект знает свой id) ---

struct Obj {
    uint8_t id = 0xFF;
    void set_id(uint8_t v) noexcept { id = v; }
};

using Ids10 = mem::WithIds<uint8_t, 10>;          // id 10…, NoId = 255
using Reg10 = mem::ObjRegistry<Obj, Ids10>;

void test_withids_ranges()
{
    static_assert(has_rebind<Reg10>::value, "WithIds: rebind есть");
    static_assert(Reg10::kFirstId == 10u && Reg10::kNoId == 255u);
    static_assert(Reg10::kMaxCapacity == 245u);

    Obj* cells[4]{};
    Reg10 r(cells, 4);
    CHECK(r.kFirstId == 10u && r.endId() == 14u);
    CHECK(r.idInRange(10) && r.idInRange(13));
    CHECK(!r.idInRange(9) && !r.idInRange(14));

    Obj* big[255]{};          // First + capacity не переполняет IdT
    Reg10 rb(big, 255);
    CHECK(rb.capacity() == 245u && rb.endId() == 255u);
    CHECK(rb.idInRange(254) && !rb.idInRange(255));
}

void test_withids_register()
{
    Obj* cells[4]{};
    Reg10 r(cells, 4);
    Obj a, b, c, d;

    CHECK(r.registerAt(12, a) == mem::RegStatus::Ok);
    CHECK(a.id == 12u && r.get(12) == &a && r.find(a) == 12u);
    CHECK(r.registerAt(12, a) == mem::RegStatus::Ok);           // тот же там же
    CHECK(r.isOk());

    CHECK(r.registerAuto(b) == mem::RegStatus::Ok);                 // первый свободный
    CHECK(b.id == 10u && r.get(10) == &b);
    CHECK(r.registerAuto(b) == mem::RegStatus::Ok);                 // повторно — no-op
    CHECK(r.count() == 2u && r.isOk());

    CHECK(r.registerAt(12, c) == mem::RegStatus::Occupied);     // первый сбой
    CHECK(r.registerAt(20, c) == mem::RegStatus::OutOfRange);
    CHECK(r.registerAt(11, a) == mem::RegStatus::Duplicate);    // a уже под 12
    CHECK(c.id == 0xFFu && a.id == 12u);
    CHECK(!r.isOk() && r.regStatus() == mem::RegStatus::Occupied);

    CHECK(r.registerAuto(c) == mem::RegStatus::Ok);
    CHECK(r.registerAuto(d) == mem::RegStatus::Ok);
    CHECK(c.id == 11u && d.id == 13u && r.isFull());
    Obj e;
    CHECK(r.registerAuto(e) == mem::RegStatus::NoFreeSlot);
    CHECK(r.find(e) == Reg10::kNoId);

    int sum_id = 0;
    r.forEach([&](Obj&, uint8_t id) { sum_id += id; });
    CHECK(sum_id == 10 + 11 + 12 + 13);
}

void test_withids_unregister_rebind()
{
    Obj* cells[4]{};
    Reg10 r(cells, 4);
    Obj a, b;
    CHECK(r.registerAt(10, a) == mem::RegStatus::Ok);
    CHECK(r.registerAt(11, b) == mem::RegStatus::Ok);

    CHECK(r.rebind(10, 13) == mem::RegStatus::Ok);                 // в пустой
    CHECK(r.get(13) == &a && a.id == 13u && r.get(10) == nullptr);

    CHECK(r.rebind(13, 11) == mem::RegStatus::Ok);                 // обмен
    CHECK(r.get(11) == &a && a.id == 11u);
    CHECK(r.get(13) == &b && b.id == 13u);
    CHECK(r.count() == 2u);

    CHECK(r.rebind(10, 12) == mem::RegStatus::NotFound);
    CHECK(r.rebind(10, 30) == mem::RegStatus::OutOfRange);

    CHECK(r.unregisterAt(13));                                     // объекту — kNoId
    CHECK(b.id == Reg10::kNoId && r.get(13) == nullptr);
    CHECK(!r.unregisterAt(13));

    CHECK(r.unregister(a));
    CHECK(a.id == Reg10::kNoId && r.count() == 0u);
    CHECK(!r.unregister(a));

    CHECK(r.registerAt(12, a) == mem::RegStatus::Ok);
    r.clear();                                                     // всем — kNoId
    CHECK(a.id == Reg10::kNoId && r.count() == 0u);
}

/** Ребёнок с явным id: видит только IIdRegistrar (без First / ёмкости). */
struct SelfObjAt : Obj {
    SelfObjAt(mem::IIdRegistrar<Obj, uint8_t>& r, uint8_t want) noexcept { r.registerAt(want, *this); }
};

/** Ребёнок без id: IRegistrar, id назначает реестр. */
struct SelfObj : Obj {
    explicit SelfObj(mem::IRegistrar<Obj>& r) noexcept { r.registerAuto(*this); }
};

void test_withids_selfreg()
{
    mem::ObjRegStore<Obj, 4, Ids10> store;
    SelfObjAt a{store, 12}, b{store, 10};
    SelfObj c{store};
    CHECK(a.id == 12u && b.id == 10u && c.id == 11u);
    CHECK(store.isOk());

    SelfObjAt d{store, 12};   // занят → первый сбой Occupied
    SelfObjAt e{store, 99};   // вне диапазона — не затирает
    CHECK(!store.isOk());
    CHECK(store.regStatus() == mem::RegStatus::Occupied);
    CHECK(d.id == 0xFFu && e.id == 0xFFu);
    CHECK(store.count() == 3u);
    CHECK(std::strcmp(mem::cstr(store.regStatus()), "Occupied") == 0);
}

/** NoId < First: id 0 — «нет id», рабочие id с 1. */
void test_withids_noid_below_first()
{
    using Ids1 = mem::WithIds<uint8_t, 1, 0>;
    mem::ObjRegStore<Obj, 2, Ids1> store;
    static_assert(decltype(store)::kNoId == 0u);
    SelfObj a{store}, b{store};
    CHECK(a.id == 1u && b.id == 2u);
    CHECK(store.unregister(a) && a.id == 0u);
}

// --- PolyStore: Base не в начале Derived ---

struct PolyOther {
    int pad = 7;
};
struct PolyBase {
    int value = 0;
};
struct PolyDerived : PolyOther, PolyBase {
    PolyDerived() noexcept { value = 42; }
};

void test_poly_base_offset()
{
    mem::PolyStore8<PolyBase, PolyDerived, 2> bank;
    CHECK(bank.baseOffset() != 0u);
    CHECK(bank.at(0) != nullptr && bank.at(0)->value == 42);
    CHECK(bank.at(1) != nullptr && bank.at(1)->value == 42);
    CHECK(bank.atDerived(0) == &bank[0]);
    CHECK(static_cast<PolyBase*>(&bank[0]) == bank.at(0));
}

} // namespace

int main()
{
    std::printf("mem::List / ObjRegistry / PolyStore checks\n");
    test_list_fifo();
    test_list_double_push();
    test_list_single();
    test_list_remove();
    test_list_push_front();
    test_list_foreach();
    test_list_clear();
    test_noids_basic();
    test_noids_selfreg();
    test_withids_ranges();
    test_withids_register();
    test_withids_unregister_rebind();
    test_withids_selfreg();
    test_withids_noid_below_first();
    test_poly_base_offset();

    std::printf("  ok=%d fail=%d\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
