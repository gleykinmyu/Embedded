/**
 * @file comm/tx_job.hpp
 * @brief comm::TxJob — пассивный дескриптор одной физической передачи.
 *
 * Job — закрытая деталь. Снаружи видны статус и isBusy.
 * Владелец задаётся конструктором и хранится в Job::owner.
 * onDone(Job&) адаптируется в onTxDone(TxJob&) внутри ITxJobOwner.
 * TxJob не знает протокол, PDU и транспорт и не хранит служебных флагов.
 * Отменить можно только задачу в Queued. После Active отмена невозможна.
 */

#pragma once

#include <cstdint>

#include "comm/job.hpp"

namespace comm {

class TxJob;

template <typename Derived>
class Transport;

class ITxJobOwner : private IJobOwner {
    friend class TxJob;

public:
    virtual void onTxReady(TxJob& job) noexcept = 0;
    virtual void onTxDone(TxJob& job) noexcept = 0;

protected:
    ITxJobOwner() = default;
    ~ITxJobOwner() = default;

private:
    void onDone(Job& job) noexcept final;
};

/** Сколько раз уничтожили TxJob, пока она была Active. */
inline uint32_t& txLifetimeFaults() noexcept
{
    static uint32_t faults = 0;
    return faults;
}

class TxJob : private Job {
public:
    using Job::Status;
    using Job::status;
    using Job::isBusy;

    explicit TxJob(ITxJobOwner& txOwner) noexcept { owner = &txOwner; }

    ~TxJob()
    {
        if (status() == Status::Active) {
            ++txLifetimeFaults();
        }
    }

private:
    template <typename>
    friend class JobList;
    template <typename>
    friend class mem::List;
    template <typename Derived>
    friend class Transport;
    friend class ITxJobOwner;

    [[nodiscard]] ITxJobOwner& txOwner() noexcept
    {
        return static_cast<ITxJobOwner&>(*owner);
    }
};

inline void ITxJobOwner::onDone(Job& job) noexcept
{
    onTxDone(static_cast<TxJob&>(job));
}

} // namespace comm
