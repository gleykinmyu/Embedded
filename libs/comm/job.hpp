/**
 * @file comm/job.hpp
 * @brief comm::Job / IJobOwner / JobList<T> — задача вызывающего и её жизненный цикл у исполнителя.
 *
 * Общий слой без сетевой специфики (txn, повторы — comm/job_net.hpp).
 * Задача принадлежит вызывающему (поле класса); исполнитель только связывает её в свой JobList.
 * Подробности результата (код отказа, сбой тела…) — в наследниках.
 *
 * Списки: _queued (ждут старта) → _active (ждут итога) → _done (ждут onDone).
 *
 * Правила:
 *   - submit занятой задачи (isBusy) → false, задача не трогается, onDone не будет;
 *   - пока isBusy — поля задачи (txn, retries, тело…) не менять (конвенция);
 *   - onDone — из tick() / cancel() / abortAll(), никогда изнутри submit / reject / finish;
 *   - перед onDone задача уже свободна: из onDone её можно сразу поставить снова;
 *   - cancel / abortAll — onDone(Aborted) сразу.
 *
 * Исполнитель:
 *   if (T* j = jobs.nextQueued()) { …начать…; jobs.start(*j, now); }
 *   по итогу:  jobs.finish(*j, Ok / Failed)
 *   tick:      jobs.tick(now)       // просроченные Active → Timeout; доставка onDone
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "mem/list.hpp"
#include "ms_timer.hpp"

namespace comm {

class Job;

/** Получатель итога задачи. */
class IJobOwner {
public:
    virtual void onDone(Job& job) noexcept = 0;

protected:
    IJobOwner() = default;
    ~IJobOwner() = default;
};

class Job : public mem::ListNode {
public:
    enum class Status : uint8_t {
        Idle = 0, ///< ни разу не ставилась
        Queued,   ///< ждёт исполнения
        Active,   ///< исполняется, ждём итога
        Ok,
        Failed,   ///< исполнитель отказал (код — в наследнике)
        Timeout,  ///< итога не дождались
        Aborted,  ///< cancel / abortAll
        Invalid,  ///< отклонена до исполнения (reject)
    };

    /** nullptr — итог только опросом status(). */
    IJobOwner* owner = nullptr;

    [[nodiscard]] Status status() const noexcept { return _status; }
    /** В очереди, исполняется или ждёт onDone. */
    [[nodiscard]] bool isBusy() const noexcept { return linked(); }
    [[nodiscard]] bool isOk() const noexcept { return _status == Status::Ok; }

private:
    template <typename>
    friend class JobList;

    Status _status = Status::Idle;
    MISC::MsTimer _timer{};
};

/** Очередь задач исполнителя: срок ожидания, итог, доставка onDone. @a T — Job или наследник. */
template <typename T = Job>
class JobList {
    static_assert(std::is_base_of_v<Job, T>, "JobList: T must derive from comm::Job");

public:
    using St = Job::Status;

    /** @a timeoutMs — сколько ждать итога после start(); 0 — без срока. */
    explicit JobList(MISC::time_t timeoutMs = 0) noexcept
        : _timeoutMs(timeoutMs)
    {}

    /** Queued в конец. @return false — задача занята. */
    [[nodiscard]] bool submit(T& job) noexcept
    {
        if (job.isBusy()) {
            return false;
        }
        base(job)._status = St::Queued;
        return _queued.pushBack(job);
    }

    /** Отклонить без исполнения: Invalid, onDone на следующем tick. @return false — занята. */
    bool reject(T& job) noexcept
    {
        if (job.isBusy()) {
            return false;
        }
        base(job)._status = St::Invalid;
        return _done.pushBack(job);
    }

    /** Нет задач в _queued и _active (_done не считается). */
    [[nodiscard]] bool empty() const noexcept
    {
        return _queued.empty() && _active.empty();
    }

    /** Голова ждущих старта. */
    [[nodiscard]] T* front() noexcept { return _queued.front(); }

    /** Первая ждущая исполнения; нет — nullptr. */
    [[nodiscard]] T* nextQueued() noexcept { return _queued.front(); }

    /** Сколько задач в Active (ждут итога). Для окна — сравнивает протокол. */
    [[nodiscard]] uint8_t activeCount() const noexcept { return _nActive; }

    /** Исполнение началось: Active, срок от @a now. @return false — задача не в _queued. */
    bool start(T& job, MISC::time_t now) noexcept
    {
        Job& b = base(job);
        if (b._status != St::Queued || !_queued.remove(job)) {
            return false;
        }
        b._status = St::Active;
        ++_nActive;
        if (_timeoutMs != 0u) {
            b._timer.start(now, _timeoutMs);
        } else {
            b._timer.stop();
        }
        // после remove узел свободен → push не может вернуть false
        _active.pushBack(job);
        return true;
    }

    /** Итог (Ok / Failed / …): снять из _queued/_active, onDone на tick. @return false — не наша / st не итог. */
    bool finish(T& job, St st) noexcept
    {
        if (st < St::Ok) {
            return false;
        }
        Job& b = base(job);
        if (b._status == St::Active) {
            if (!_active.remove(job)) {
                return false;
            }
            --_nActive;
        } else if (b._status == St::Queued) {
            if (!_queued.remove(job)) {
                return false;
            }
        } else {
            return false;
        }
        b._status = st;
        _done.pushBack(job);
        return true;
    }

    /**
     * Снять одну задачу из _queued/_active → Aborted, onDone сразу.
     * @return false — не в работе (Idle / уже в _done / чужая).
     */
    bool cancel(T& job) noexcept
    {
        Job& b = base(job);
        if (b._status == St::Active) {
            if (!_active.remove(job)) {
                return false;
            }
            --_nActive;
        } else if (b._status == St::Queued) {
            if (!_queued.remove(job)) {
                return false;
            }
        } else {
            return false;
        }
        b._status = St::Aborted;
        _done.pushBack(job);
        deliver();
        return true;
    }

    /** Просроченные Active → Timeout; затем onDone всем завершённым. */
    void tick(MISC::time_t now) noexcept
    {
        expire(now, [&](T& j) { finish(j, St::Timeout); });
        deliver();
    }

    /** Всем в _queued/_active — Aborted; onDone сразу (и ранее завершённым). */
    void abortAll() noexcept
    {
        while (T* j = _queued.popFront()) {
            base(*j)._status = St::Aborted;
            _done.pushBack(*j);
        }
        while (T* j = _active.popFront()) {
            --_nActive;
            base(*j)._status = St::Aborted;
            _done.pushBack(*j);
        }
        deliver();
    }

protected:
    /** Первая Active, для которой @a pred(const T&) == true. */
    template <typename Pred>
    [[nodiscard]] T* findActive(Pred&& pred) noexcept
    {
        T* found = nullptr;
        _active.forEach([&](T& j) {
            if (!pred(static_cast<const T&>(j))) {
                return false;
            }
            found = &j;
            return true;
        });
        return found;
    }

    /** Первая Queued, для которой @a pred(const T&) == true. */
    template <typename Pred>
    [[nodiscard]] T* findQueued(Pred&& pred) noexcept
    {
        T* found = nullptr;
        _queued.forEach([&](T& j) {
            if (!pred(static_cast<const T&>(j))) {
                return false;
            }
            found = &j;
            return true;
        });
        return found;
    }

    /** Для каждой просроченной Active — fn(T&); fn может снять её (finish) или вернуть (requeue). */
    template <typename Fn>
    void expire(MISC::time_t now, Fn&& fn) noexcept
    {
        _active.forEach([&](T& j) {
            if (base(j)._timer.timedOut(now)) {
                fn(j);
            }
        });
    }

    /** Active → голова _queued (повтор раньше новых). */
    void requeue(T& job) noexcept
    {
        Job& b = base(job);
        if (b._status != St::Active || !_active.remove(job)) {
            return;
        }
        --_nActive;
        b._status = St::Queued;
        b._timer.stop();
        _queued.pushFront(job);
    }

    /**
     * Снимок _done: поставленные из onDone (reject) — на следующем tick.
     * Отдельный batch, а не «до хвоста»: onDone может вызвать tick / abortAll (вложенный deliver).
     */
    void deliver() noexcept
    {
        mem::List<T> batch;
        while (T* j = _done.popFront()) {
            batch.pushBack(*j);
        }
        while (T* j = batch.popFront()) {
            if (j->owner != nullptr) {
                j->owner->onDone(*j);
            }
        }
    }

private:
    [[nodiscard]] static Job& base(T& j) noexcept { return j; }

    mem::List<T> _queued;
    mem::List<T> _active;
    mem::List<T> _done;
    const MISC::time_t _timeoutMs;
    uint8_t _nActive = 0;
};

} // namespace comm
