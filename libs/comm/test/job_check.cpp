/**
 * @file job_check.cpp
 * @brief Unit-тесты comm::JobList<T> (общий слой) и comm::JobListNet (txn, повторы).
 *
 *   make -C libs/comm/test job-check
 */

#include <cstdint>
#include <cstdio>

#include "comm/job.hpp"
#include "comm/job_net.hpp"

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

/* Сетевой слой — большинство тестов; общий — test_base_*. */
using Job = comm::JobNet;
using JobList = comm::JobListNet<>;
using St = comm::Job::Status;

constexpr MISC::time_t kTimeout = 100u;

/** Считает onDone; опционально ставит задачу снова из onDone. */
struct Owner : comm::IJobOwner {
    int calls = 0;
    comm::Job* last = nullptr;
    St lastStatus = St::Idle;
    JobList* resubmitTo = nullptr;
    bool resubmitOk = false;
    bool rejectFromDone = false;

    void onDone(comm::Job& done) noexcept override
    {
        ++calls;
        last = &done;
        lastStatus = done.status();
        if (resubmitTo != nullptr) {
            JobList* const to = resubmitTo;
            resubmitTo = nullptr;
            Job& j = static_cast<Job&>(done);
            resubmitOk = rejectFromDone ? to->reject(j) : to->submit(j);
        }
    }
};

// --- общий слой ---

void test_base_timeout_no_retry()
{
    std::puts("CASE base: start → таймаут → Timeout (повторов нет)");
    comm::JobList<> jobs{kTimeout};
    Owner o;
    comm::Job a;
    a.owner = &o;
    CHECK(jobs.submit(a));
    CHECK(jobs.nextQueued() == &a);
    CHECK(jobs.start(a, 0u));
    CHECK(a.status() == St::Active && jobs.nextQueued() == nullptr);
    jobs.tick(kTimeout - 1u);
    CHECK(o.calls == 0);
    jobs.tick(kTimeout);
    CHECK(o.calls == 1 && o.lastStatus == St::Timeout && !a.isBusy());
}

void test_base_no_deadline()
{
    std::puts("CASE base: timeoutMs = 0 — без срока, итог только finish");
    comm::JobList<> jobs;
    Owner o;
    comm::Job a;
    a.owner = &o;
    CHECK(jobs.submit(a) && jobs.start(a, 0u));
    jobs.tick(0xFFFFFFFFu);
    CHECK(o.calls == 0 && a.status() == St::Active);
    CHECK(jobs.finish(a, St::Failed));
    jobs.tick(0u);
    CHECK(o.calls == 1 && o.lastStatus == St::Failed);
}

/** Наследник с полезной нагрузкой: очередь отдаёт T*, без приведения. */
struct WriteJob : comm::Job {
    int block = 0;
};

void test_base_typed()
{
    std::puts("CASE base: JobList<WriteJob> отдаёт WriteJob*");
    comm::JobList<WriteJob> jobs{kTimeout};
    WriteJob w;
    w.block = 42;
    CHECK(jobs.submit(w));
    WriteJob* const n = jobs.nextQueued();
    CHECK(n == &w && n->block == 42);
}

// --- сетевой слой ---

void test_submit_busy()
{
    std::puts("CASE submit / busy");
    JobList jobs{kTimeout};
    Job a;
    a.txn = 1u;
    CHECK(a.status() == St::Idle && !a.isBusy());
    CHECK(jobs.submit(a));
    CHECK(a.status() == St::Queued && a.isBusy() && jobs.front() == &a);
    CHECK(!jobs.submit(a)); // занята → false, статус не тронут
    CHECK(a.status() == St::Queued);
}

void test_ok_path()
{
    std::puts("CASE start → findTxn → finish(Ok) → onDone на tick");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    a.txn = 7u;
    CHECK(jobs.submit(a));
    jobs.start(a, 1000u);
    CHECK(a.status() == St::Active && a.attempts() == 1u && a.txn == 7u);
    CHECK(jobs.activeCount() == 1u);
    CHECK(jobs.findTxn(8u) == nullptr);
    CHECK(jobs.findTxn(7u) == &a);
    CHECK(jobs.finish(a, St::Ok));
    CHECK(o.calls == 0);        // не изнутри finish
    CHECK(a.isBusy() && jobs.empty() && jobs.activeCount() == 0u);
    jobs.tick(1001u);
    CHECK(o.calls == 1 && o.last == &a && o.lastStatus == St::Ok);
    CHECK(!a.isBusy() && a.isOk());
    CHECK(!jobs.finish(a, St::Ok)); // уже не в очереди
}

void test_findtxn_only_active()
{
    std::puts("CASE findTxn — только Active");
    JobList jobs{kTimeout};
    Job a;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    CHECK(jobs.findTxn(1u) == nullptr); // Queued не находится
    CHECK(jobs.activeCount() == 0u);
}

void test_retry_then_timeout()
{
    std::puts("CASE таймаут → повтор с тем же txn → Timeout");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    a.retries = 1u;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    jobs.start(a, 0u);
    jobs.tick(kTimeout - 1u);
    CHECK(a.status() == St::Active && o.calls == 0);
    jobs.tick(kTimeout);
    CHECK(a.status() == St::Queued && jobs.front() == &a); // повтор
    jobs.start(a, kTimeout);
    CHECK(a.attempts() == 2u && a.txn == 1u && jobs.findTxn(1u) == &a);
    jobs.tick(2u * kTimeout);
    CHECK(o.calls == 1 && o.lastStatus == St::Timeout && !a.isBusy());
}

void test_no_retry()
{
    std::puts("CASE retries = 0 → сразу Timeout");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    jobs.start(a, 0u);
    jobs.tick(kTimeout);
    CHECK(o.calls == 1 && o.lastStatus == St::Timeout);
}

void test_deadline_wrap()
{
    std::puts("CASE дедлайн через переполнение uint32");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    const MISC::time_t t0 = 0xFFFFFFF0u;
    jobs.start(a, t0);
    jobs.tick(t0 + 50u); // переполнилось, но ещё рано
    CHECK(o.calls == 0 && a.status() == St::Active);
    jobs.tick(t0 + kTimeout);
    CHECK(o.calls == 1 && o.lastStatus == St::Timeout);
}

void test_deadline_hits_zero()
{
    std::puts("CASE now + timeout == 0 (kStopped) — таймер всё равно идёт");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    jobs.start(a, 0u - kTimeout); // срок попадает ровно на 0 → сдвиг на 1 мс
    jobs.tick(0u);
    CHECK(o.calls == 0 && a.status() == St::Active);
    jobs.tick(1u);
    CHECK(o.calls == 1 && o.lastStatus == St::Timeout);
}

void test_reject_deferred()
{
    std::puts("CASE reject → Invalid на следующем tick");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    CHECK(jobs.reject(a));
    CHECK(o.calls == 0 && a.isBusy() && a.status() == St::Invalid && jobs.empty());
    CHECK(!jobs.submit(a)); // ждёт доставки — занята
    jobs.tick(0u);
    CHECK(o.calls == 1 && o.lastStatus == St::Invalid && !a.isBusy());
}

void test_resubmit_from_ondone()
{
    std::puts("CASE повторная постановка из onDone");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    jobs.start(a, 0u);
    CHECK(jobs.finish(a, St::Failed));
    o.resubmitTo = &jobs;
    jobs.tick(1u);
    CHECK(o.calls == 1 && o.lastStatus == St::Failed);
    CHECK(o.resubmitOk && a.status() == St::Queued && jobs.front() == &a);
    CHECK(a.attempts() == 0u);
}

void test_reject_from_ondone_no_loop()
{
    std::puts("CASE reject из onDone — доставка на следующем tick, без цикла");
    JobList jobs{kTimeout};
    Owner o;
    Job a;
    a.owner = &o;
    CHECK(jobs.reject(a));
    o.resubmitTo = &jobs;
    o.rejectFromDone = true;
    jobs.tick(0u);
    CHECK(o.calls == 1 && o.resubmitOk && a.isBusy());
    jobs.tick(1u);
    CHECK(o.calls == 2 && !a.isBusy());
}

void test_abort_all()
{
    std::puts("CASE abortAll → Aborted сразу, плюс уже завершённые");
    JobList jobs{kTimeout};
    Owner o;
    Job a, b, c;
    a.owner = b.owner = c.owner = &o;
    a.txn = b.txn = c.txn = 1u;
    CHECK(jobs.submit(a) && jobs.submit(b) && jobs.submit(c));
    jobs.start(a, 0u);
    CHECK(jobs.finish(c, St::Ok)); // ждёт tick
    jobs.abortAll();
    CHECK(o.calls == 3 && jobs.empty());
    CHECK(a.status() == St::Aborted && b.status() == St::Aborted && c.status() == St::Ok);
    CHECK(!a.isBusy() && !b.isBusy() && !c.isBusy());
}

void test_fifo_and_middle()
{
    std::puts("CASE порядок FIFO, finish из середины");
    JobList jobs{kTimeout};
    Job a, b, c;
    a.txn = b.txn = c.txn = 1u;
    CHECK(jobs.submit(a) && jobs.submit(b) && jobs.submit(c));
    CHECK(jobs.front() == &a);
    CHECK(jobs.finish(b, St::Aborted));
    CHECK(jobs.finish(a, St::Ok));
    CHECK(jobs.front() == &c);
    jobs.tick(0u); // owner == nullptr — просто освобождаются
    CHECK(!a.isBusy() && !b.isBusy() && c.isBusy());
}

void test_next_queued_window2()
{
    std::puts("CASE две в полёте: повтор второй виден через nextQueued");
    JobList jobs{kTimeout};
    Job a, b;
    a.txn = 1u;
    b.txn = 2u;
    b.retries = 1u;
    CHECK(jobs.submit(a) && jobs.submit(b));
    CHECK(jobs.nextQueued() == &a);
    CHECK(jobs.start(a, 50u));
    CHECK(jobs.activeCount() == 1u && jobs.nextQueued() == &b);
    CHECK(jobs.start(b, 0u));
    CHECK(jobs.activeCount() == 2u && jobs.nextQueued() == nullptr);
    jobs.tick(kTimeout); // b просрочена, a ещё нет
    CHECK(a.status() == St::Active && b.status() == St::Queued);
    CHECK(jobs.nextQueued() == &b && jobs.activeCount() == 1u);
    CHECK(jobs.findTxn(1u) == &a && jobs.findTxn(2u) == nullptr); // b ждёт повтора
}

void test_misuse_guards()
{
    std::puts("CASE защита: start не-Queued, finish не-итогом");
    JobList jobs{kTimeout};
    Job a, idle;
    CHECK(!jobs.start(idle, 0u) && idle.status() == St::Idle);
    a.txn = 1u;
    CHECK(jobs.submit(a));
    CHECK(jobs.start(a, 0u));
    CHECK(!jobs.start(a, 10u) && a.txn == 1u && a.attempts() == 1u); // уже Active
    CHECK(!jobs.finish(a, St::Queued) && !jobs.finish(a, St::Active) && !jobs.finish(a, St::Idle));
    CHECK(a.status() == St::Active && jobs.findTxn(1u) == &a && jobs.nextQueued() == nullptr);
}

/** onDone закрывает очередь (abortAll) и отклоняет задачу снова — без зацикливания. */
struct ReentrantOwner : comm::IJobOwner {
    JobList* jobs = nullptr;
    int calls = 0;
    void onDone(comm::Job& done) noexcept override
    {
        ++calls;
        jobs->abortAll();      // вложенная доставка
        (void)jobs->reject(static_cast<Job&>(done));
    }
};

void test_reentrant_deliver()
{
    std::puts("CASE onDone → abortAll + reject: вложенная доставка без цикла");
    JobList jobs{kTimeout};
    ReentrantOwner o;
    o.jobs = &jobs;
    Job a, b;
    a.owner = b.owner = &o;
    CHECK(jobs.reject(a) && jobs.reject(b));
    jobs.tick(0u);
    CHECK(o.calls >= 2 && o.calls < 10);
    const int after = o.calls;
    jobs.tick(1u);
    CHECK(o.calls > after && o.calls < after + 10);
}

void test_no_owner()
{
    std::puts("CASE owner == nullptr — итог опросом");
    JobList jobs{kTimeout};
    Job a;
    a.txn = 1u;
    CHECK(jobs.submit(a));
    jobs.start(a, 0u);
    CHECK(jobs.finish(a, St::Ok));
    jobs.tick(0u);
    CHECK(a.isOk() && !a.isBusy());
}

void test_cancel()
{
    std::puts("CASE cancel Queued / Active → Aborted сразу; Idle/done → false");
    JobList jobs{kTimeout};
    Owner o;
    Job a, b, idle;
    a.owner = b.owner = &o;
    a.txn = 1u;
    b.txn = 2u;
    CHECK(!jobs.cancel(idle));
    CHECK(jobs.submit(a) && jobs.submit(b));
    CHECK(jobs.start(a, 0u));
    CHECK(jobs.cancel(b)); // ещё Queued
    CHECK(o.calls == 1 && o.lastStatus == St::Aborted && !b.isBusy());
    CHECK(jobs.cancel(a)); // Active
    CHECK(o.calls == 2 && a.status() == St::Aborted && !a.isBusy() && jobs.activeCount() == 0u);
    CHECK(jobs.finish(a, St::Ok) == false);
    CHECK(!jobs.cancel(a)); // уже свободна
    CHECK(jobs.reject(b));
    CHECK(!jobs.cancel(b)); // уже в _done
    jobs.tick(0u);
    CHECK(o.calls == 3 && o.lastStatus == St::Invalid);
}

} // namespace

int main()
{
    std::puts("comm::JobList / JobListNet");
    test_base_timeout_no_retry();
    test_base_no_deadline();
    test_base_typed();
    test_submit_busy();
    test_ok_path();
    test_findtxn_only_active();
    test_retry_then_timeout();
    test_no_retry();
    test_deadline_wrap();
    test_deadline_hits_zero();
    test_reject_deferred();
    test_resubmit_from_ondone();
    test_reject_from_ondone_no_loop();
    test_abort_all();
    test_fifo_and_middle();
    test_next_queued_window2();
    test_misuse_guards();
    test_reentrant_deliver();
    test_no_owner();
    test_cancel();
    std::printf("  ok=%d fail=%d\n", g_ok, g_fail);
    return g_fail == 0 ? 0 : 1;
}
