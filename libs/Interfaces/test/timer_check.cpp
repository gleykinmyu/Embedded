/**
 * @file timer_check.cpp
 * @brief Хостовые тесты MISC::TimerService / EvMsTimer / EvRetryTimer.
 *
 *   make -C libs/Interfaces/test
 *   make -C libs/Interfaces/test timer-check
 */

#include <cstdint>
#include <cstdio>
#include <new>
#include <type_traits>

#include "ev_timer.hpp"
#include "ms_timer.hpp"

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

using MISC::EvMsTimer;
using MISC::EvRetryTimer;
using MISC::IRetryTimerListener;
using MISC::ITimerListener;
using MISC::TimerService;
using MISC::time_t;

static_assert(!std::is_copy_constructible<EvMsTimer>::value, "timer address is identity");
static_assert(!std::is_move_constructible<EvMsTimer>::value, "timer address is identity");
static_assert(!std::is_copy_assignable<EvMsTimer>::value, "timer address is identity");
static_assert(!std::is_move_assignable<EvMsTimer>::value, "timer address is identity");
static_assert(!std::is_copy_constructible<EvRetryTimer>::value, "retry timer is not copyable");
static_assert(!std::is_move_constructible<EvRetryTimer>::value, "retry timer is not movable");
static_assert(!std::is_copy_constructible<TimerService>::value, "service is not copyable");
static_assert(!std::is_move_constructible<TimerService>::value, "service is not movable");

struct Hits : ITimerListener {
    int hits = 0;
    EvMsTimer* last = nullptr;

    void onTimer(EvMsTimer& timer) noexcept override
    {
        ++hits;
        last = &timer;
    }
};

void test_expire_and_cancel()
{
    std::puts("CASE истечение, отмена, чужой ожидающий, два сервиса");

    TimerService svc;
    Hits hits;
    EvMsTimer t{svc, hits};
    CHECK(!t.isRunning());
    CHECK(!t.isPending());

    t.start(0u, 100u);
    CHECK(t.isRunning());
    svc.tick(99u);
    CHECK(hits.hits == 0);
    CHECK(t.isRunning());
    svc.tick(100u);
    CHECK(hits.hits == 1);
    CHECK(hits.last == &t);
    CHECK(!t.isRunning());
    CHECK(!t.isPending());
    svc.tick(100u);
    CHECK(hits.hits == 1);
    t.stop();
    CHECK(hits.hits == 1);

    t.start(100u, 50u);
    CHECK(t.isRunning());
    t.stop();
    t.stop();
    CHECK(!t.isRunning());
    svc.tick(150u);
    CHECK(hits.hits == 1);

    struct KillLater : ITimerListener {
        EvMsTimer* soon = nullptr;
        EvMsTimer* later = nullptr;
        int soonHits = 0;
        int laterHits = 0;

        void onTimer(EvMsTimer& timer) noexcept override
        {
            if (&timer == soon) {
                ++soonHits;
                CHECK(later->isRunning());
                CHECK(!later->isPending());
                later->stop();
                CHECK(!later->isRunning());
            } else {
                ++laterHits;
            }
        }
    };

    KillLater kill;
    EvMsTimer soon{svc, kill};
    EvMsTimer later{svc, kill};
    kill.soon = &soon;
    kill.later = &later;
    soon.start(0u, 10u);
    later.start(0u, 40u);
    svc.tick(10u);
    CHECK(kill.soonHits == 1);
    CHECK(kill.laterHits == 0);
    CHECK(!later.isRunning());
    svc.tick(40u);
    svc.tick(100u);
    CHECK(kill.laterHits == 0);

    struct Order : ITimerListener {
        EvMsTimer* seq[2]{};
        int n = 0;

        void onTimer(EvMsTimer& timer) noexcept override { seq[n++] = &timer; }
    };
    Order order;
    EvMsTimer first{svc, order};
    EvMsTimer second{svc, order};
    first.start(0u, 5u);
    second.start(0u, 5u);
    svc.tick(5u);
    CHECK(order.n == 2);
    CHECK(order.seq[0] == &first);
    CHECK(order.seq[1] == &second);

    Order sooner;
    EvMsTimer slow{svc, sooner};
    EvMsTimer fast{svc, sooner};
    slow.start(0u, 30u);
    fast.start(0u, 10u);
    svc.tick(10u);
    CHECK(sooner.n == 1);
    CHECK(sooner.seq[0] == &fast);
    CHECK(slow.isRunning());
    svc.tick(30u);
    CHECK(sooner.n == 2);
    CHECK(sooner.seq[1] == &slow);

    TimerService other;
    Hits foreign;
    EvMsTimer there{other, foreign};
    there.start(0u, 5u);
    svc.tick(5u);
    CHECK(foreign.hits == 0);
    CHECK(there.isRunning());
    other.tick(5u);
    CHECK(foreign.hits == 1);
    CHECK(!there.isRunning());
}

void test_restart()
{
    std::puts("CASE перезапуск: новый срок встаёт по порядку дедлайнов");

    TimerService svc;
    Hits hits;
    EvMsTimer t{svc, hits};
    t.start(0u, 100u);
    svc.tick(50u);
    CHECK(hits.hits == 0);
    t.start(50u, 100u);
    CHECK(t.isRunning());
    svc.tick(100u);
    CHECK(hits.hits == 0);
    CHECK(t.isRunning());
    svc.tick(149u);
    CHECK(hits.hits == 0);
    svc.tick(150u);
    CHECK(hits.hits == 1);
    CHECK(!t.isRunning());

    struct Order : ITimerListener {
        EvMsTimer* seq[2]{};
        int n = 0;
        void onTimer(EvMsTimer& timer) noexcept override { seq[n++] = &timer; }
    };
    Order order;
    EvMsTimer a{svc, order};
    EvMsTimer b{svc, order};
    a.start(0u, 80u);
    b.start(0u, 50u);
    a.start(0u, 40u);
    svc.tick(40u);
    CHECK(order.n == 1);
    CHECK(order.seq[0] == &a);
    CHECK(b.isRunning());
    svc.tick(50u);
    CHECK(order.n == 2);
    CHECK(order.seq[1] == &b);

    Order equal;
    EvMsTimer early{svc, equal};
    EvMsTimer again{svc, equal};
    early.start(0u, 20u);
    again.start(0u, 20u);
    early.start(0u, 20u);
    svc.tick(20u);
    CHECK(equal.n == 2);
    CHECK(equal.seq[0] == &again);
    CHECK(equal.seq[1] == &early);

    struct Supersede : ITimerListener {
        EvMsTimer* keep = nullptr;
        EvMsTimer* moved = nullptr;
        int keepHits = 0;
        int movedHits = 0;
        time_t now = 0;

        void onTimer(EvMsTimer& timer) noexcept override
        {
            if (&timer == keep) {
                ++keepHits;
                CHECK(moved->isPending());
                CHECK(!moved->isRunning());
                moved->start(now, 50u);
                CHECK(moved->isRunning());
                CHECK(!moved->isPending());
            } else {
                ++movedHits;
            }
        }
    };
    Supersede sup;
    EvMsTimer keep{svc, sup};
    EvMsTimer moved{svc, sup};
    sup.keep = &keep;
    sup.moved = &moved;
    keep.start(0u, 10u);
    moved.start(0u, 10u);
    sup.now = 10u;
    svc.tick(10u);
    CHECK(sup.keepHits == 1);
    CHECK(sup.movedHits == 0);
    CHECK(moved.isRunning());
    svc.tick(10u);
    svc.tick(59u);
    CHECK(sup.movedHits == 0);
    svc.tick(60u);
    CHECK(sup.movedHits == 1);
    CHECK(sup.keepHits == 1);
}

void test_rearm_self()
{
    std::puts("CASE повторный start себя из callback и вложенный tick");

    struct Self : ITimerListener {
        TimerService* svc = nullptr;
        EvMsTimer* self = nullptr;
        EvMsTimer* other = nullptr;
        int selfHits = 0;
        int otherHits = 0;
        time_t now = 0;

        void onTimer(EvMsTimer& timer) noexcept override
        {
            if (&timer == self) {
                ++selfHits;
                CHECK(!timer.isRunning());
                CHECK(!timer.isPending());
                const int seen = selfHits;
                svc->tick(now);
                CHECK(selfHits == seen);
                if (seen == 1) {
                    CHECK(other->isPending());
                    timer.start(now, 10u);
                    CHECK(timer.isRunning());
                    CHECK(!timer.isPending());
                    svc->tick(now);
                    CHECK(selfHits == seen);
                    CHECK(other->isPending());
                }
            } else {
                ++otherHits;
                CHECK(&timer == other);
                CHECK(!timer.isRunning());
            }
        }
    };

    TimerService svc;
    Self self;
    EvMsTimer a{svc, self};
    EvMsTimer b{svc, self};
    self.svc = &svc;
    self.self = &a;
    self.other = &b;
    a.start(0u, 10u);
    b.start(0u, 10u);
    self.now = 10u;
    svc.tick(10u);
    CHECK(self.selfHits == 1);
    CHECK(self.otherHits == 1);
    CHECK(a.isRunning());
    CHECK(!b.isRunning());

    svc.tick(10u);
    svc.tick(19u);
    CHECK(self.selfHits == 1);
    CHECK(self.otherHits == 1);
    svc.tick(20u);
    CHECK(self.selfHits == 2);
    CHECK(self.otherHits == 1);
    CHECK(!a.isRunning());
}

void test_start_other()
{
    std::puts("CASE start другого таймера из callback — не в этом проходе");

    struct Pair : ITimerListener {
        EvMsTimer* a = nullptr;
        EvMsTimer* b = nullptr;
        int aHits = 0;
        int bHits = 0;
        time_t now = 0;

        void onTimer(EvMsTimer& timer) noexcept override
        {
            if (&timer == a) {
                ++aHits;
                CHECK(!a->isRunning());
                CHECK(!b->isRunning());
                CHECK(!b->isPending());
                b->start(now, 0u);
                CHECK(b->isRunning());
                CHECK(!b->isPending());
                CHECK(bHits == 0);
            } else {
                ++bHits;
                CHECK(&timer == b);
            }
        }
    };

    TimerService svc;
    Pair pair;
    EvMsTimer a{svc, pair};
    EvMsTimer b{svc, pair};
    pair.a = &a;
    pair.b = &b;
    a.start(0u, 10u);
    pair.now = 10u;
    svc.tick(10u);
    CHECK(pair.aHits == 1);
    CHECK(pair.bHits == 0);
    CHECK(b.isRunning());
    svc.tick(10u);
    CHECK(pair.aHits == 1);
    CHECK(pair.bHits == 1);
    CHECK(!b.isRunning());
    svc.tick(10u);
    CHECK(pair.bHits == 1);
}

void test_cancel_pending()
{
    std::puts("CASE отмена таймера, уже стоящего в очереди callback");

    struct Cancel : ITimerListener {
        EvMsTimer* killer = nullptr;
        EvMsTimer* victim = nullptr;
        int killerHits = 0;
        int victimHits = 0;

        void onTimer(EvMsTimer& timer) noexcept override
        {
            if (&timer == killer) {
                ++killerHits;
                CHECK(victim->isPending());
                CHECK(!victim->isRunning());
                victim->stop();
                CHECK(!victim->isPending());
                CHECK(!victim->isRunning());
                victim->stop();
            } else {
                ++victimHits;
            }
        }
    };

    TimerService svc;
    Cancel cancel;
    EvMsTimer killer{svc, cancel};
    EvMsTimer victim{svc, cancel};
    cancel.killer = &killer;
    cancel.victim = &victim;
    killer.start(0u, 10u);
    victim.start(0u, 10u);
    svc.tick(10u);
    CHECK(cancel.killerHits == 1);
    CHECK(cancel.victimHits == 0);
    CHECK(!victim.isPending());
    CHECK(!victim.isRunning());
    svc.tick(10u);
    svc.tick(1000u);
    CHECK(cancel.victimHits == 0);
    CHECK(cancel.killerHits == 1);
}

void test_retry()
{
    std::puts("CASE retry: попытка, исчерпание, clear, stop не сбрасывает счётчик");

    struct RetryL : IRetryTimerListener {
        EvRetryTimer* timer = nullptr;
        time_t now = 0;
        time_t delay = 10;
        int retries = 0;
        int exhausted = 0;
        uint8_t seen[4]{};

        void onTimer(EvMsTimer& timer) noexcept override
        {
            CHECK(&timer == this->timer);
            CHECK(!timer.isRunning());
            CHECK(!this->timer->isAttemptsOver());
            seen[retries] = this->timer->attempts();
            ++retries;
            timer.start(now, delay);
        }

        void onAttemptsOver(EvRetryTimer& timer) noexcept override
        {
            CHECK(&timer == this->timer);
            CHECK(timer.isAttemptsOver());
            CHECK(!timer.isRunning());
            CHECK(!timer.isPending());
            ++exhausted;
        }
    };

    TimerService svc;
    RetryL retry;
    EvRetryTimer t{svc, retry, 3u};
    retry.timer = &t;
    CHECK(t.attempts() == 0u);
    CHECK(t.maxAttempts() == 3u);
    CHECK(!t.isAttemptsOver());

    t.start(0u, 10u);
    CHECK(t.attempts() == 1u);
    CHECK(t.isRunning());
    CHECK(!t.isAttemptsOver());

    retry.now = 10u;
    svc.tick(10u);
    CHECK(retry.retries == 1);
    CHECK(retry.seen[0] == 1u);
    CHECK(retry.exhausted == 0);
    CHECK(t.attempts() == 2u);
    CHECK(t.isRunning());

    retry.now = 20u;
    svc.tick(20u);
    CHECK(retry.retries == 2);
    CHECK(retry.seen[1] == 2u);
    CHECK(retry.exhausted == 0);
    CHECK(t.attempts() == 3u);
    CHECK(t.isAttemptsOver());
    CHECK(t.isRunning());

    retry.now = 30u;
    svc.tick(30u);
    CHECK(retry.retries == 2);
    CHECK(retry.exhausted == 1);
    CHECK(t.attempts() == 3u);
    CHECK(!t.isRunning());
    svc.tick(40u);
    CHECK(retry.retries == 2);
    CHECK(retry.exhausted == 1);

    t.clear();
    CHECK(t.attempts() == 0u);
    CHECK(!t.isAttemptsOver());
    CHECK(!t.isRunning());
    t.start(40u, 10u);
    CHECK(t.attempts() == 1u);
    retry.now = 50u;
    svc.tick(50u);
    CHECK(retry.retries == 3);
    CHECK(t.attempts() == 2u);
    CHECK(retry.exhausted == 1);

    TimerService oneShot;
    RetryL once;
    EvRetryTimer single{oneShot, once, 1u};
    once.timer = &single;
    once.delay = 5u;
    single.start(0u, 5u);
    CHECK(single.attempts() == 1u);
    CHECK(single.isAttemptsOver());
    CHECK(single.isRunning());
    once.now = 5u;
    oneShot.tick(5u);
    CHECK(once.retries == 0);
    CHECK(once.exhausted == 1);
    CHECK(!single.isRunning());

    TimerService paused;
    RetryL stopped;
    EvRetryTimer hold{paused, stopped, 3u};
    stopped.timer = &hold;
    hold.start(0u, 100u);
    CHECK(hold.attempts() == 1u);
    hold.stop();
    CHECK(hold.attempts() == 1u);
    CHECK(!hold.isRunning());
    paused.tick(100u);
    CHECK(stopped.retries == 0);
    CHECK(stopped.exhausted == 0);
    hold.start(100u, 10u);
    CHECK(hold.attempts() == 2u);
    CHECK(hold.isRunning());
}

void test_overflow()
{
    std::puts("CASE переполнение time_t, сверка с MsTimer");

    auto expectFire = [](time_t arm, time_t dur, const time_t* quiet, int nQuiet, time_t fireAt) {
        MISC::MsTimer legacy;
        legacy.start(arm, dur);
        for (int i = 0; i < nQuiet; ++i) {
            CHECK(!legacy.timedOut(quiet[i]));
        }
        CHECK(legacy.timedOut(fireAt));

        TimerService svc;
        Hits hits;
        EvMsTimer t{svc, hits};
        t.start(arm, dur);
        CHECK(t.isRunning());
        for (int i = 0; i < nQuiet; ++i) {
            svc.tick(quiet[i]);
            CHECK(hits.hits == 0);
            CHECK(t.isRunning());
        }
        svc.tick(fireAt);
        CHECK(hits.hits == 1);
        CHECK(hits.last == &t);
        CHECK(!t.isRunning());
        svc.tick(fireAt);
        CHECK(hits.hits == 1);
    };

    const time_t wrapQuiet[] = {0xFFFFFFF0u, 0xFFFFFFFFu, 0u, 0x0Fu};
    expectFire(0xFFFFFFF0u, 0x20u, wrapQuiet, 4, 0x10u);

    const time_t bumpQuiet[] = {0xFFFFFFF0u, 0xFFFFFFFFu, 0u};
    expectFire(0xFFFFFFF0u, 0x10u, bumpQuiet, 3, 1u);

    const time_t edgeQuiet[] = {0xFFFFFFFEu};
    expectFire(0xFFFFFFFEu, 1u, edgeQuiet, 1, 0xFFFFFFFFu);

    const time_t halfQuiet[] = {0u, 0x7FFFFFFEu};
    expectFire(0u, 0x7FFFFFFFu, halfQuiet, 2, 0x7FFFFFFFu);

    const time_t zeroQuiet[] = {4u};
    expectFire(5u, 0u, zeroQuiet, 1, 5u);

    struct Order : ITimerListener {
        EvMsTimer* seq[2]{};
        int n = 0;
        void onTimer(EvMsTimer& timer) noexcept override { seq[n++] = &timer; }
    };
    TimerService svc;
    Order order;
    EvMsTimer later{svc, order};
    EvMsTimer sooner{svc, order};
    later.start(0xFFFFFFF0u, 0x20u);
    sooner.start(0xFFFFFFF0u, 0x08u);
    svc.tick(0xFFFFFFF8u);
    CHECK(order.n == 1);
    CHECK(order.seq[0] == &sooner);
    CHECK(later.isRunning());
    svc.tick(0x10u);
    CHECK(order.n == 2);
    CHECK(order.seq[1] == &later);
}

void test_service_dtor()
{
    std::puts("CASE деструктор сервиса снимает таймер без callback");

    Hits hits;
    alignas(TimerService) unsigned char raw[sizeof(TimerService)];
    auto* svc = new (raw) TimerService();
    EvMsTimer t{*svc, hits};
    t.start(0u, 5u);
    CHECK(t.isRunning());
    svc->~TimerService();
    CHECK(!t.isRunning());
    CHECK(!t.isPending());
    CHECK(hits.hits == 0);
}

} // namespace

int main()
{
    std::puts("MISC::TimerService / EvMsTimer / EvRetryTimer");
    test_expire_and_cancel();
    test_restart();
    test_rearm_self();
    test_start_other();
    test_cancel_pending();
    test_retry();
    test_overflow();
    test_service_dtor();
    std::printf("  ok=%d fail=%d\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
