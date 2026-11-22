/**
 * @file new_smcp/proto/multiframe.hpp
 * @brief Длинное тело одним SeqData: seq 0 — чанки и CRC32, seq ≥ 1 — байты.
 *
 * Задачу ставит вызывающий и он же IJobOwner. Буфер известен до первого кадра.
 * TX шлёт buf, RX пишет в buf. Окно одно. Другой расклад SeqData не принимается.
 */

#pragma once

#include <cstdint>

#include "comm/old/container.hpp"
#include "comm/job.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/ibuf.hpp"
#include "smcp/new_smcp/session.hpp"

namespace smcp::proto {

struct FrameJob : comm::Job {
    enum class Error : uint8_t {
        None = 0,
        Seq,
        Overflow,
        Crc,
        Unexpected,
        Abort,
        Timeout,
    };

    enum class Dir : uint8_t { Rx = 0, Tx };

    IBuf buf{}; ///< TX: что слать. RX: куда принять. Задан до submit.
    uint8_t txn = 0;
    Dir dir = Dir::Rx;
    Error error = Error::None;
    uint16_t got = 0; ///< TX: длина тела; RX: принято байт
};

class MultiFrame : public comm::Proto<Frame> {
public:
    using Error = FrameJob::Error;

    explicit MultiFrame(Session& session) noexcept;

    /**
     * TX шлёт job.buf, RX принимает в job.buf.
     * @return false — сессия закрыта, задача занята, txn 0 или буфер пуст.
     */
    [[nodiscard]] bool submit(FrameJob& job) noexcept;

    [[nodiscard]] bool onPacket(const Frame& pkt, MISC::time_t now) noexcept override;
    void tick(MISC::time_t now) noexcept override;
    void abort() noexcept override;

protected:
    [[nodiscard]] bool peekTx(Frame& out, MISC::time_t) noexcept override;
    void onTxDone(const Frame&, bool ok) noexcept override;

private:
    class Jobs : public comm::JobList<FrameJob> {
    public:
        using comm::JobList<FrameJob>::JobList;
        void flush() noexcept { deliver(); }

        [[nodiscard]] FrameJob* findQueuedTxn(uint8_t txn) noexcept
        {
            return findQueued([txn](const FrameJob& j) {
                return j.dir == FrameJob::Dir::Rx && j.txn == txn;
            });
        }
    };

    enum class Phase : uint8_t { Idle = 0, Header, Body };

    enum class TxState : uint8_t { Idle = 0, Ready, Packed, Sent };

    void beginTx(FrameJob& job) noexcept;
    void beginRx(FrameJob& job, MISC::time_t now) noexcept;
    void driveTx() noexcept;
    void armGap(MISC::time_t now) noexcept;

    [[nodiscard]] uint8_t chunkLen(uint16_t seq) const noexcept;

    void succeed() noexcept;
    void fail(Error err) noexcept;
    void clearActive() noexcept;

    Session& _session;
    Jobs _jobs;
    FrameJob* _job = nullptr;
    Phase _phase = Phase::Idle;
    TxState _txState = TxState::Idle;
    uint16_t _chunks = 0;
    uint16_t _seq = 0;
    uint16_t _got = 0;
    uint32_t _crc = 0;
    MISC::MsTimer _gap{};
};

} // namespace smcp::proto
