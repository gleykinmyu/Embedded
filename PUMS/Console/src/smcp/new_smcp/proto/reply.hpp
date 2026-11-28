/**
 * @file new_smcp/proto/reply.hpp
 * @brief Short RPC: request → Ack|Nack (окно 1, JobListNet).
 *
 * Вызывающий владеет RequestJob; submit → peekTx → onTxDone(start) → Ack|Nack|Timeout.
 */

#pragma once

#include <cstdint>

#include "comm/container.hpp"
#include "comm/job_net.hpp"
#include "smcp/new_smcp/session.hpp"

namespace smcp::proto {

/** Задача short RPC: исходящий proto::msg::Header + итог Nack при Failed. */
struct RequestJob : comm::JobNet {
    proto::msg::Header req{};
    Nack nack{}; ///< при Failed / Timeout (error=Timeout)
};

/**
 * Протокол ответов на сессии.
 * Окно Active = 1: пока ждём Ack|Nack, следующий Queued не уходит на wire.
 */
class Reply : public comm::Proto<Packet>, private comm::IJobOwner {
public:
    using Jobs = comm::JobListNet<RequestJob>;
    using St = comm::Job::Status;

    static constexpr uint8_t kWindow = 1u;

    explicit Reply(Session& session) noexcept
        : Proto(session)
        , _session(session)
        , _jobs(Ack::kTimeoutMs)
    {}

    [[nodiscard]] Jobs& jobs() noexcept { return _jobs; }
    [[nodiscard]] const Jobs& jobs() const noexcept { return _jobs; }
    [[nodiscard]] bool isWaiting() const noexcept { return _jobs.activeCount() > 0u; }

    /**
     * Поставить short RPC. Заполняет txn / retries.
     * @return false — занята / сессия закрыта / нет свободного txn.
     */
    [[nodiscard]] bool submit(RequestJob& job) noexcept
    {
        if (!_session.isOpen() || job.isBusy()) {
            return false;
        }
        const uint8_t txn = allocTxn();
        if (txn == 0u) {
            return false;
        }
        job.txn = txn;
        job.retries = Ack::kRetry;
        job.nack = {};
        if (job.owner == nullptr) {
            job.owner = this; // Timeout приходит через IJobOwner::onDone
        }
        return _jobs.submit(job);
    }

    [[nodiscard]] bool onPacket(const Packet& pkt, MISC::time_t) noexcept override
    {
        const proto::msg::Id id = pkt.msg.id;
        if (id != proto::msg::Id::Ack && id != proto::msg::Id::Nack) {
            return false;
        }
        // Active после onTxDone; Queued/_wire — если Ack пришёл раньше start.
        RequestJob* j = _jobs.findTxnBusy(pkt.txn);
        if (j == nullptr) {
            return false;
        }
        SMCP_IF_MSG(Nack) {
            j->nack = body;
            if (_jobs.finish(*j, St::Failed)) {
                _session.onNack(j->txn, j->nack.error, j->nack.detail);
            }
        } else SMCP_IF_MSG(Ack) {
            if (_jobs.finish(*j, St::Ok)) {
                _session.onAck(j->txn);
            }
        } else {
            return false;
        }
        if (_wire == j) {
            _wire = nullptr;
        }
        return true;
    }

    void tick(MISC::time_t now) noexcept override
    {
        _now = now;
        _jobs.tick(now);
    }

    void abort() noexcept override
    {
        _wire = nullptr;
        _jobs.abortAll();
    }

protected:
    [[nodiscard]] bool peekTx(Packet& out, MISC::time_t now) noexcept override
    {
        _now = now;
        if (!_session.isOpen() || _wire != nullptr || _jobs.activeCount() >= kWindow) {
            return false;
        }
        RequestJob* j = _jobs.nextQueued();
        if (j == nullptr) {
            return false;
        }
        out = {};
        out.dst = _session.peer();
        out.txn = j->txn;
        out.msg = j->req;
        _wire = j;
        return true;
    }

    void onTxDone(const Packet& frame, bool ok) noexcept override
    {
        RequestJob* j = _wire;
        _wire = nullptr;
        if (j == nullptr) {
            j = _jobs.findTxn(frame.txn);
        }
        if (j == nullptr) {
            return;
        }
        if (!ok) {
            j->nack.error = Err::Body;
            j->nack.detail = Nack::kDetailNone;
            if (_jobs.finish(*j, St::Failed)) {
                _session.onNack(j->txn, j->nack.error, j->nack.detail);
            }
            return;
        }
        if (j->status() == St::Queued) {
            (void)_jobs.start(*j, _now);
        }
    }

private:
    void onDone(comm::Job& job) noexcept override
    {
        if (job.status() != St::Timeout) {
            return;
        }
        auto& req = static_cast<RequestJob&>(job);
        req.nack.error = Err::Timeout;
        req.nack.detail = Nack::kDetailNone;
        _session.onNack(req.txn, Err::Timeout, Nack::kDetailNone);
    }

    [[nodiscard]] uint8_t allocTxn() noexcept
    {
        for (uint8_t n = 0; n < proto::msg::kTxnIdMax; ++n) {
            uint8_t t = _nextTxn++;
            if (_nextTxn == 0u || _nextTxn > proto::msg::kTxnIdMax) {
                _nextTxn = 1u;
            }
            if (t == 0u) {
                continue;
            }
            if (_jobs.findTxnBusy(t) == nullptr) {
                return t;
            }
        }
        return 0;
    }

    Session& _session;
    Jobs _jobs;
    RequestJob* _wire = nullptr;
    uint8_t _nextTxn = 1;
    MISC::time_t _now = 0;
};

} // namespace smcp::proto
