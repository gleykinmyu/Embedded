/**
 * @file comm/job_net.hpp
 * @brief comm::JobNet / JobListNet — сетевой слой задач: номер транзакции, повторы по таймауту.
 *
 * Протокол:
 *   постановка: job.txn = nextTxn++; jobs.submit(job)   // txn один на запуск: сервер отсекает дубли
 *   writeTx:  if (JobNet* j = jobs.nextQueued()) → кадр с j->txn
 *   onTxDone: jobs.start(*j, now)
 *   onPacket: if (JobNet* j = jobs.findTxn(txn)) jobs.finish(*j, Ok / Failed)   // Failed — Nack
 *   tick:     jobs.tick(now)       // таймаут → повтор (снова Queued) или Timeout; доставка
 */

#pragma once

#include <cstdint>

#include "comm/job.hpp"

namespace comm {

class JobNet : public Job {
public:
    /** Повторов после первой попытки. */
    uint8_t retries = 0;
    /** txn запуска (общий для всех попыток); задаёт вызывающий до submit. */
    uint8_t txn = 0;

    /** Отправок в текущем запуске. */
    [[nodiscard]] uint8_t attempts() const noexcept { return _attempts; }

private:
    template <typename>
    friend class JobListNet;

    uint8_t _attempts = 0;
};

/** JobList + txn + повторы. @a T — JobNet или наследник (RequestJob, ObjJob). */
template <typename T = JobNet>
class JobListNet : public JobList<T> {
    static_assert(std::is_base_of_v<JobNet, T>, "JobListNet: T must derive from comm::JobNet");
    using Base = JobList<T>;

public:
    using typename Base::St;
    using Base::Base;

    /** Queued в конец, счётчик попыток с нуля. @return false — задача занята. */
    [[nodiscard]] bool submit(T& job) noexcept
    {
        if (!Base::submit(job)) {
            return false;
        }
        net(job)._attempts = 0;
        return true;
    }

    /** Кадр ушёл: Active, срок, ++attempts. @return false — задача не Queued. */
    bool start(T& job, MISC::time_t now) noexcept
    {
        if (!Base::start(job, now)) {
            return false;
        }
        ++net(job)._attempts;
        return true;
    }

    /** Active с этим txn; нет — nullptr. */
    [[nodiscard]] T* findTxn(uint8_t txn) noexcept
    {
        return Base::findActive([txn](const T& j) { return j.txn == txn; });
    }

    /** Active или Queued с этим txn (allocTxn / ранний ответ). */
    [[nodiscard]] T* findTxnBusy(uint8_t txn) noexcept
    {
        if (T* j = findTxn(txn)) {
            return j;
        }
        return Base::findQueued([txn](const T& j) { return j.txn == txn; });
    }

    /** Просроченные Active → повтор (Queued) или Timeout; затем onDone всем завершённым. */
    void tick(MISC::time_t now) noexcept
    {
        Base::expire(now, [&](T& j) {
            if (net(j)._attempts <= j.retries) {
                Base::requeue(j);
            } else {
                (void)Base::finish(j, St::Timeout);
            }
        });
        Base::deliver();
    }

private:
    [[nodiscard]] static JobNet& net(T& j) noexcept { return j; }
};

} // namespace comm
