/**
 * @file comm/transport.hpp
 * @brief comm::Transport — очередь TxJob и одна текущая выдача ресурса.
 *
 * CRTP вызывает физические операции конкретного транспорта без virtual.
 * Общий слой не знает число mailbox и не хранит таблицу слотов.
 * Принятый кадр связан с TxJob указателем в TxSlot конкретного транспорта.
 * Общий слой слотами не владеет и тип задачи из void* не восстанавливает.
 *
 * Успешный transmit ещё не завершает задачу: итог приходит из pollTx.
 * Повторный send в том же onTxReady разрешён, пока transmit не принял кадр.
 * После принятия повторный send возвращает false и принятую передачу не трогает.
 * send чужой задачи и send вне onTxReady — false.
 *
 * JobList создан с таймаутом 0. Срок физической передачи снимает задачу
 * только конкретный транспорт, вместе с обнулением TxSlot::job.
 *
 * cancelTx снимает только Queued: сразу Aborted и onTxDone.
 * Переход в Active — точка, после которой отмена невозможна, в том числе
 * для задачи, чей onTxReady выполняется прямо сейчас.
 *
 * Порядок tick:
 *   1. onTick(now) и завершения уже принятых передач.
 *   2. onTxDone этих завершений — до приёма.
 *   3. Приём, не больше лимита конкретного транспорта.
 *   4. Выдача ресурса. Бюджет равен txSpace() на входе в выдачу
 *      и уменьшается на каждый onTxReady, даже если send не принял кадр.
 *   5. onTxDone задач, завершённых на этом шаге без передачи.
 * Повторный вход в tick этого экземпляра ничего не делает.
 * close() из callback транспорта — нарушение контракта: он не выполняется,
 * счётчик transportCloseFaults() растёт и в Release.
 *
 * close() снаружи tick просит драйвер отпустить TxSlot::job принятых кадров.
 * pollTx приносит их собственный TxResult, он не подменяется на Aborted.
 * Оставшиеся Queued снимает abortAll как Aborted.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "comm/tx_job.hpp"

namespace comm {

enum class TxResult : uint8_t { Ok, Failed, Timeout };

/** Сколько раз close() вызвали из callback этого транспорта. */
inline uint32_t& transportCloseFaults() noexcept
{
    static uint32_t faults = 0;
    return faults;
}

template <typename Derived>
class Transport {
public:
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    [[nodiscard]] bool requestTx(TxJob& job) noexcept
    {
        if (_closed || job.isBusy()) {
            return false;
        }
        return _jobs.submit(job);
    }

    /** Только Queued. Active, включая задачу внутри onTxReady, не отменяется. */
    bool cancelTx(TxJob& job) noexcept
    {
        if (job.status() != Job::Status::Queued) {
            return false;
        }
        return _jobs.cancel(job);
    }

    /**
     * Один кадр в рамках onTxReady этой же задачи.
     * @return false — не текущая задача, повтор после принятия или отказ драйвера.
     */
    template <typename Frame>
    bool send(TxJob& job, const Frame& frame)
    {
        if (_closed || _current != &job || _sent) {
            return false;
        }
        _sent = self().transmit(job, frame);
        return _sent;
    }

    /**
     * Один проход обслуживания. Верхний уровень — аргумент, транспорт его не хранит.
     * @a Upper предоставляет onReceive(Reader&).
     */
    template <typename Upper>
    void tick(Upper& upper, MISC::time_t now)
    {
        if (_closed || _inTick) {
            return;
        }
        _inTick = true;
        self().onTick(now);
        serviceCompleted();
        _jobs.tick(now);
        self().receive([&](auto& reader) { upper.onReceive(reader); });
        serviceTx(now);
        _jobs.tick(now);
        _inTick = false;
    }

    void close() noexcept
    {
        if (_inTick) {
            ++transportCloseFaults();
            return;
        }
        if (_closed) {
            return;
        }
        _closed = true;
        self().shutdown();
        serviceCompleted();
        _jobs.abortAll();
    }

    [[nodiscard]] bool isOpen() const noexcept { return !_closed; }

protected:
    Transport() = default;
    ~Transport() = default;

    [[nodiscard]] Derived& self() noexcept { return static_cast<Derived&>(*this); }
    [[nodiscard]] const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

private:
    static Job::Status mapResult(TxResult result) noexcept
    {
        switch (result) {
        case TxResult::Ok:
            return Job::Status::Ok;
        case TxResult::Timeout:
            return Job::Status::Timeout;
        case TxResult::Failed:
            return Job::Status::Failed;
        }
        return Job::Status::Failed;
    }

    void serviceCompleted() noexcept
    {
        TxJob* job = nullptr;
        TxResult result{};
        while (self().pollTx(job, result)) {
            assert(job != nullptr);
            (void)_jobs.finish(*job, mapResult(result));
        }
    }

    void serviceTx(MISC::time_t now)
    {
        auto budget = self().txSpace();
        while (budget != 0u) {
            TxJob* job = _jobs.nextQueued();
            if (job == nullptr || !_jobs.start(*job, now)) {
                break;
            }
            _current = job;
            _sent = false;
            job->txOwner().onTxReady(*job);
            _current = nullptr;
            --budget;
            if (job->status() == Job::Status::Active && !_sent) {
                (void)_jobs.finish(*job, Job::Status::Failed);
            }
        }
    }

    JobList<TxJob> _jobs{0};
    TxJob* _current = nullptr;
    bool _sent = false;
    bool _closed = false;
    bool _inTick = false;
};

} // namespace comm
