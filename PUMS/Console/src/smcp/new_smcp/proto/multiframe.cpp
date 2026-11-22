/**
 * @file new_smcp/proto/multiframe.cpp
 * @brief Тела MultiFrame в порядке объявления.
 */

#include "smcp/new_smcp/proto/multiframe.hpp"

#include <cstring>

#include "crc.hpp"
#include "smcp/new_smcp/pdu.hpp"

namespace smcp::proto {

MultiFrame::MultiFrame(Session& session) noexcept
    : Proto(session)
    , _session(session)
    , _jobs(0u)
{}

bool MultiFrame::submit(FrameJob& job) noexcept
{
    if (!_session.isOpen() || job.isBusy() || job.txn == 0u) {
        return false;
    }
    if (job.buf.data == nullptr || job.buf.size == 0u) {
        return false;
    }
    job.error = Error::None;
    job.got = 0;
    return _jobs.submit(job);
}

bool MultiFrame::onPacket(const Frame& pkt, MISC::time_t now) noexcept
{
    if (!_session.isOpen()) {
        return false;
    }
    SMCP_IF_MSG(SeqData) {
        if (_job == nullptr) {
            FrameJob* queued = _jobs.findQueuedTxn(pkt.txn);
            if (queued == nullptr) {
                return false; //TODO подумать над ответом на неизвестный txn
            }
            beginRx(*queued, now);
        }
        if (_job->dir == FrameJob::Dir::Tx || pkt.txn != _job->txn) {
            return false;
        }

        if (body.seq == 0u) {
            if (_phase != Phase::Header || body.first.chunks == 0u) {
                fail(Error::Unexpected);
                return true;
            }
            const uint32_t cap = static_cast<uint32_t>(body.first.chunks) * SeqData::kPayload;
            if (cap > 0xFFFFu || _job->buf.data == nullptr) {
                fail(Error::Overflow);
                return true;
            }
            _chunks = body.first.chunks;
            _crc = body.first.crc32.value();
            _seq = 1;
            _got = 0;
            _phase = Phase::Body;
            armGap(now);
            return true;
        }

        if (_phase != Phase::Body || body.seq != _seq) {
            fail(Error::Seq);
            return true;
        }
        const bool last = _seq == _chunks;
        if ((!last && body.chunk.len != SeqData::kPayload) || (last && body.chunk.len == 0u)) {
            fail(Error::Unexpected);
            return true;
        }
        const uint32_t next = static_cast<uint32_t>(_got) + body.chunk.len;
        if (_job->buf.data == nullptr || next > _job->buf.size) {
            fail(Error::Overflow);
            return true;
        }
        std::memcpy(_job->buf.data + _got, body.chunk.data, body.chunk.len);
        _got = static_cast<uint16_t>(next);
        ++_seq;
        if (!last) {
            armGap(now);
            return true;
        }
        const uint32_t crc = MISC::Crc32::compute(_job->buf.data, _got);
        if (crc != _crc) {
            fail(Error::Crc);
            return true;
        }
        _job->got = _got;
        succeed();
        return true;
    }
    return false;
}

void MultiFrame::tick(MISC::time_t now) noexcept
{
    if (_job == nullptr) {
        FrameJob* next = _jobs.nextQueued();
        if (next == nullptr) {
            return;
        }
        if (next->dir == FrameJob::Dir::Tx) {
            beginTx(*next);
        } else {
            beginRx(*next, now);
        }
        return;
    }
    if (_job->dir == FrameJob::Dir::Tx) {
        driveTx();
        return;
    }
    if (_gap.timedOut(now)) {
        fail(Error::Timeout);
    }
}

void MultiFrame::abort() noexcept
{
    if (_job != nullptr && _job->error == Error::None) {
        _job->error = Error::Abort;
    }
    clearActive();
    _jobs.abortAll();
}

bool MultiFrame::peekTx(Frame& out, MISC::time_t) noexcept
{
    if (_txState != TxState::Ready || _job == nullptr || _job->dir != FrameJob::Dir::Tx
        || !_session.isOpen()) {
        return false;
    }
    SeqData frame{};
    frame.seq = _seq;
    if (_seq == 0u) {
        frame.first.chunks = _chunks;
        frame.first.crc32.setValue(_crc);
    } else {
        frame.chunk.len = chunkLen(_seq);
        const uint16_t off = static_cast<uint16_t>((_seq - 1u) * SeqData::kPayload);
        if (frame.chunk.len != 0u) {
            std::memcpy(frame.chunk.data, _job->buf.data + off, frame.chunk.len);
        }
    }
    out = {};
    out.dst = _session.peer();
    out.txn = _job->txn;
    frame.pack(out);
    _txState = TxState::Packed;
    return true;
}

void MultiFrame::onTxDone(const Frame&, bool ok) noexcept
{
    if (_txState != TxState::Packed) {
        return;
    }
    _txState = ok ? TxState::Sent : TxState::Ready;
}

void MultiFrame::beginTx(FrameJob& job) noexcept
{
    _jobs.start(job, 0u);
    _job = &job;
    _chunks = static_cast<uint16_t>((job.buf.size + SeqData::kPayload - 1u) / SeqData::kPayload);
    _crc = MISC::Crc32::compute(job.buf.data, job.buf.size);
    _seq = 0;
    _got = 0;
    _phase = Phase::Header;
    _txState = TxState::Ready;
}

void MultiFrame::beginRx(FrameJob& job, MISC::time_t now) noexcept
{
    _jobs.start(job, now);
    _job = &job;
    _chunks = 0;
    _seq = 0;
    _got = 0;
    _crc = 0;
    _phase = Phase::Header;
    _txState = TxState::Idle;
    armGap(now);
}

void MultiFrame::driveTx() noexcept
{
    if (_txState != TxState::Sent || _job == nullptr) {
        return;
    }
    _txState = TxState::Idle;
    if (_phase == Phase::Header) {
        _phase = Phase::Body;
        _seq = 1;
        _txState = TxState::Ready;
        return;
    }
    _got = static_cast<uint16_t>(_got + chunkLen(_seq));
    ++_seq;
    if (_seq > _chunks) {
        _job->got = _job->buf.size;
        succeed();
        return;
    }
    _txState = TxState::Ready;
}

void MultiFrame::armGap(MISC::time_t now) noexcept
{
    _gap.start(now, SeqData::kTimeoutMs);
}

uint8_t MultiFrame::chunkLen(uint16_t seq) const noexcept
{
    if (_job == nullptr || seq == 0u) {
        return 0;
    }
    const uint32_t off = static_cast<uint32_t>(seq - 1u) * SeqData::kPayload;
    if (off >= _job->buf.size) {
        return 0;
    }
    const uint32_t left = _job->buf.size - off;
    return left > SeqData::kPayload ? SeqData::kPayload : static_cast<uint8_t>(left);
}

void MultiFrame::succeed() noexcept
{
    FrameJob* job = _job;
    clearActive();
    if (job == nullptr) {
        return;
    }
    job->error = Error::None;
    _jobs.finish(*job, comm::Job::Status::Ok);
    _jobs.flush();
}

void MultiFrame::fail(Error err) noexcept
{
    FrameJob* job = _job;
    clearActive();
    if (job == nullptr) {
        return;
    }
    job->error = err;
    _jobs.finish(*job, comm::Job::Status::Failed);
    _jobs.flush();
}

void MultiFrame::clearActive() noexcept
{
    _job = nullptr;
    _phase = Phase::Idle;
    _txState = TxState::Idle;
    _chunks = 0;
    _seq = 0;
    _got = 0;
    _crc = 0;
    _gap.stop();
}

} // namespace smcp::proto
