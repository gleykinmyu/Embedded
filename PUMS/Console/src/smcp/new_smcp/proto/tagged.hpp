/**
 * @file new_smcp/proto/tagged.hpp
 * @brief Get / Set opener + короткое тело; длинное — MultiFrame (SeqData).
 *
 * Клиент: get/set → onReply / onSetAck.
 * Сервер: onGet / onSetOpen+receive → sendBody / Ack|Nack.
 * Короткий Set копируется через onSetBuffer. Длинный — буфер в receive до SeqData.
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "comm/old/container.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/ibuf.hpp"
#include "smcp/new_smcp/transport.hpp"
#include "smcp/new_smcp/pdu.hpp"
#include "smcp/new_smcp/proto/multiframe.hpp"

namespace smcp::proto {

/** Прикладной слой поверх Tagged. */
struct ITaggedSink {
    /** Входящий Get (сервер): ответить sendBody или Nack. */
    virtual void onGet(const Get& body, uint8_t txn) noexcept = 0;

    /**
     * Короткий Set: отдать буфер ≥ size.
     * @return false — NotSupported.
     */
    [[nodiscard]] virtual bool onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn,
                                           uint16_t size, IBuf& buf) noexcept = 0;

    /** Set без тела: договорились о SeqData. Приём — receive(buf) из этого вызова. */
    virtual void onSetOpen(uint8_t tag, uint8_t index, uint8_t txn) noexcept
    {
        (void)tag;
        (void)index;
        (void)txn;
    }

    /** Тело Set принято (транспорт Ok). Err::None → Ack, иначе Nack. */
    [[nodiscard]] virtual Err onSet(uint8_t tag, uint8_t index,
                                    const IBuf& buf) noexcept = 0;

    /** Итог своего Get (клиент). */
    virtual void onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                         MultiFrame::Error err) noexcept = 0;

    /** Итог своего Set (клиент): ok или Nack. */
    virtual void onSetAck(uint8_t tag, uint8_t index, bool ok,
                          const Nack& nack) noexcept = 0;

protected:
    ~ITaggedSink() = default;
};

class Tagged : public comm::Proto<Frame>, private comm::IJobOwner {
public:
    enum class State : uint8_t {
        Idle = 0,
        GetAsk,
        GetReply,
        SetIn,
        SetOutInline,
        SetOut,
        SetOutAwaitAck,
    };

    enum class Result : uint8_t { Ok = 0, Busy, Fail };

    Tagged(SessionT& session, MultiFrame& xfer, ITaggedSink* sink = nullptr) noexcept
        : Proto(session)
        , _session(session)
        , _xfer(xfer)
        , _sink(sink)
        , _ack(Ack::kRetry)
    {}

    void setSink(ITaggedSink* sink) noexcept { _sink = sink; }

    [[nodiscard]] bool isBusy() const noexcept { return _state != State::Idle; }
    [[nodiscard]] State state() const noexcept { return _state; }

    [[nodiscard]] Result set(uint8_t tag, const IBuf& buf,
                             uint8_t index = kIndexAll) noexcept
    {
        if (!_session.isOpen() || buf.size == 0u || buf.data == nullptr) {
            return Result::Fail;
        }
        if (isBusy()) {
            return Result::Busy;
        }
        const uint8_t txn = allocTxn();
        if (txn == 0u) {
            return Result::Fail;
        }

        if (buf.size <= kInlineMax) {
            Set body{};
            body.tag = tag;
            body.index = index;
            body.len = static_cast<uint8_t>(buf.size);
            std::memcpy(body.data, buf.data, body.len);
            body.pack(_pend.msg);
            _pend.txn = txn;
            _pend.tag = tag;
            _pend.index = index;
            _pend.sent = false;
            _pend.kind = Pend::SetOpener;
            _state = State::SetOutInline;
            return Result::Ok;
        }

        _txBuf = buf;
        _deferTx = true;
        Set body{};
        body.tag = tag;
        body.index = index;
        body.len = 0;
        body.pack(_pend.msg);
        _pend.txn = txn;
        _pend.tag = tag;
        _pend.index = index;
        _pend.sent = false;
        _pend.kind = Pend::SetOpener;
        _state = State::SetOut;
        return Result::Ok;
    }

    [[nodiscard]] Result get(uint8_t tag, IBuf& buf,
                             uint8_t index = kIndexAll) noexcept
    {
        if (!_session.isOpen() || buf.data == nullptr || buf.size == 0u) {
            return Result::Fail;
        }
        if (isBusy()) {
            return Result::Busy;
        }
        const uint8_t txn = allocTxn();
        if (txn == 0u) {
            return Result::Fail;
        }
        _pend.txn = txn;
        _pend.tag = tag;
        _pend.index = index;
        if (!submitRx(buf)) {
            _pend = {};
            return Result::Fail;
        }
        Get body{};
        body.tag = tag;
        body.index = index;
        body.pack(_pend.msg);
        _pend.txn = txn;
        _pend.tag = tag;
        _pend.index = index;
        _pend.sent = false;
        _pend.kind = Pend::GetOpener;
        _state = State::GetAsk;
        return Result::Ok;
    }

    /** Ответ на Get: всегда SeqData, даже короткое тело. */
    [[nodiscard]] bool sendBody(uint8_t tag, const IBuf& buf, uint8_t txn,
                                uint8_t index = kIndexAll) noexcept
    {
        if (!_session.isOpen() || txn == 0u || buf.size == 0u || buf.data == nullptr) {
            return false;
        }
        if (isBusy()) {
            return false;
        }
        _pend.txn = txn;
        _pend.tag = tag;
        _pend.index = index;
        if (!submitTx(buf)) {
            return false;
        }
        _state = State::GetReply;
        return true;
    }

    /** Приём SeqData в уже известный буфер. Зовут из onSetOpen. */
    [[nodiscard]] bool receive(const IBuf& buf) noexcept
    {
        if (_state != State::SetIn || _job.isBusy()) {
            return false;
        }
        return submitRx(buf);
    }

    void sendAck(uint8_t txn) noexcept
    {
        queueOut(txn, [](Frame& m) { Ack{}.pack(m); });
    }

    void sendNack(uint8_t txn, Err err, uint32_t detail = Nack::kDetailNone) noexcept
    {
        Nack n{};
        n.error = err;
        n.detail = detail;
        queueOut(txn, [&](Frame& m) { n.pack(m); });
    }

    [[nodiscard]] bool onPacket(const Frame& pkt, MISC::time_t now) noexcept override
    {
        _now = now;
        if (!_session.isOpen()) {
            return false;
        }
        const uint8_t txn = pkt.txn;

        SMCP_IF_MSG(Ack) {
            if (_state != State::SetOutAwaitAck || txn != _pend.txn) {
                return false;
            }
            const uint8_t tag = _pend.tag;
            const uint8_t index = _pend.index;
            reset();
            if (_sink != nullptr) {
                _sink->onSetAck(tag, index, true, {});
            }
            return true;
        }
        SMCP_IF_MSG(Nack) {
            if (txn != _pend.txn) {
                return false;
            }
            if (isSetOut()) {
                const uint8_t tag = _pend.tag;
                const uint8_t index = _pend.index;
                const Nack n = body;
                reset();
                if (_sink != nullptr) {
                    _sink->onSetAck(tag, index, false, n);
                }
                return true;
            }
            if (_state == State::GetAsk) {
                failGet(MultiFrame::Error::Abort);
                return true;
            }
            return false;
        }
        SMCP_IF_MSG(Get) {
            if (_sink != nullptr) {
                _sink->onGet(body, txn);
            }
            return true;
        }
        SMCP_IF_MSG(Set) {
            if (isBusy()) {
                sendNack(txn, Err::Busy);
                return true;
            }
            if (body.len == 0u) {
                _pend.txn = txn;
                _pend.tag = body.tag;
                _pend.index = body.index;
                _state = State::SetIn;
                if (_sink != nullptr) {
                    _sink->onSetOpen(body.tag, body.index, txn);
                }
                return true;
            }
            IBuf buf{};
            if (_sink == nullptr
                || !_sink->onSetBuffer(body.tag, body.index, txn, body.len, buf)
                || buf.size < body.len || buf.data == nullptr) {
                sendNack(txn, Err::NotSupported);
                return true;
            }
            std::memcpy(buf.data, body.data, body.len);
            buf.size = body.len;
            finishSetIn(body.tag, body.index, buf, MultiFrame::Error::None, txn);
            return true;
        }
        return false;
    }

    void tick(MISC::time_t now) noexcept override
    {
        _now = now;
        if (_state == State::SetOutAwaitAck && _ack.timedOut(now)) {
            if (_ack.isAttemptsOver()) {
                Nack n{};
                n.error = Err::Timeout;
                const uint8_t tag = _pend.tag;
                const uint8_t index = _pend.index;
                reset();
                if (_sink != nullptr) {
                    _sink->onSetAck(tag, index, false, n);
                }
            } else {
                _ack.start(now, Ack::kTimeoutMs);
            }
        }
    }

    void abort() noexcept override
    {
        if (_job.isBusy()) {
            _xfer.abort();
            return;
        }
        if (_state == State::GetAsk) {
            failGet(MultiFrame::Error::Abort);
            return;
        }
        if (isSetOut()) {
            Nack n{};
            n.error = Err::Body;
            n.detail = static_cast<uint32_t>(MultiFrame::Error::Abort);
            const uint8_t tag = _pend.tag;
            const uint8_t index = _pend.index;
            resetLocal();
            if (_sink != nullptr) {
                _sink->onSetAck(tag, index, false, n);
            }
            return;
        }
        resetLocal();
    }

protected:
    [[nodiscard]] bool peekTx(Frame& out, MISC::time_t now) noexcept override
    {
        _now = now;
        if (_inFlight || !_session.isOpen()) {
            return false;
        }
        if (_outPending) {
            out = _out;
            out.dst = _session.peer();
            _inFlight = true;
            _wireOut = true;
            return true;
        }
        if (_pend.kind != Pend::None
            && (_state == State::GetAsk || _state == State::SetOutInline
                || _state == State::SetOut)
            && !_pend.sent) {
            out = {};
            out.dst = _session.peer();
            out.txn = _pend.txn;
            out.copyBody(_pend.msg);
            _inFlight = true;
            _wireOut = false;
            return true;
        }
        return false;
    }

    void onTxDone(const Frame&, bool ok) noexcept override
    {
        _inFlight = false;
        if (_wireOut) {
            _wireOut = false;
            if (ok) {
                _outPending = false;
            }
            return;
        }
        if (_pend.kind == Pend::None || _pend.sent) {
            return;
        }
        _pend.sent = true;
        if (!ok) {
            if (_state == State::GetAsk) {
                failGet(MultiFrame::Error::Abort);
            } else if (isSetOut()) {
                Nack n{};
                n.error = Err::Body;
                const uint8_t tag = _pend.tag;
                const uint8_t index = _pend.index;
                reset();
                if (_sink != nullptr) {
                    _sink->onSetAck(tag, index, false, n);
                }
            }
            return;
        }
        if (_state == State::SetOutInline) {
            beginAck(_now);
            return;
        }
        if (_state == State::SetOut && _deferTx) {
            _deferTx = false;
            if (!submitTx(_txBuf)) {
                Nack n{};
                n.error = Err::Body;
                const uint8_t tag = _pend.tag;
                const uint8_t index = _pend.index;
                resetLocal();
                if (_sink != nullptr) {
                    _sink->onSetAck(tag, index, false, n);
                }
            }
        }
    }

private:
    enum class Pend : uint8_t { None = 0, GetOpener, SetOpener };

    struct PendSlot {
        Pend kind = Pend::None;
        uint8_t txn = 0;
        uint8_t tag = 0;
        uint8_t index = 0;
        bool sent = false;
        Message msg{};
    };

    [[nodiscard]] bool isSetOut() const noexcept
    {
        return _state == State::SetOutInline || _state == State::SetOut
            || _state == State::SetOutAwaitAck;
    }

    void reset() noexcept
    {
        if (_job.isBusy()) {
            _xfer.abort();
            return;
        }
        resetLocal();
    }

    void resetLocal() noexcept
    {
        _state = State::Idle;
        _pend = {};
        _ack.clear();
        _outPending = false;
        _deferTx = false;
        _txBuf = {};
    }

    void beginAck(MISC::time_t now) noexcept
    {
        _state = State::SetOutAwaitAck;
        _ack.start(now, Ack::kTimeoutMs);
    }

    void readOpenerTag() noexcept
    {
        if (_pend.kind == Pend::GetOpener) {
            Get g{};
            if (g.unpack(_pend.msg)) {
                _pend.tag = g.tag;
                _pend.index = g.index;
            }
        } else if (_pend.kind == Pend::SetOpener) {
            Set s{};
            if (s.unpack(_pend.msg)) {
                _pend.tag = s.tag;
                _pend.index = s.index;
            }
        }
    }

    void failGet(MultiFrame::Error err) noexcept
    {
        if (_job.isBusy()) {
            _job.error = err;
            _xfer.abort();
            return;
        }
        readOpenerTag();
        const uint8_t tag = _pend.tag;
        const uint8_t index = _pend.index;
        resetLocal();
        if (_sink != nullptr) {
            _sink->onReply(tag, index, {}, err);
        }
    }

    void finishSetIn(uint8_t tag, uint8_t index, const IBuf& buf,
                     MultiFrame::Error err, uint8_t txn) noexcept
    {
        if (err != MultiFrame::Error::None) {
            if (txn != 0u) {
                sendNack(txn, Err::Body, static_cast<uint32_t>(err));
            }
            return;
        }
        const Err code = (_sink != nullptr) ? _sink->onSet(tag, index, buf) : Err::None;
        if (txn == 0u) {
            return;
        }
        if (code == Err::None) {
            sendAck(txn);
        } else {
            sendNack(txn, code);
        }
    }

    template <typename Fn>
    void queueOut(uint8_t txn, Fn&& packMsg) noexcept
    {
        _out = {};
        _out.txn = txn;
        packMsg(_out);
        _outPending = true;
    }

    [[nodiscard]] uint8_t allocTxn() noexcept
    {
        for (uint8_t n = 0; n < kTxnIdMax; ++n) {
            uint8_t t = _nextTxn++;
            if (_nextTxn == 0u || _nextTxn > kTxnIdMax) {
                _nextTxn = 1u;
            }
            if (t != 0u) {
                return t;
            }
        }
        return 0;
    }

    void onDone(comm::Job& job) noexcept override
    {
        auto& frame = static_cast<FrameJob&>(job);
        const MultiFrame::Error err = frame.status() == comm::Job::Status::Ok
            ? MultiFrame::Error::None
            : (frame.error == MultiFrame::Error::None ? MultiFrame::Error::Abort : frame.error);
        const IBuf view{frame.buf.data, frame.got};
        const uint8_t tag = _pend.tag;
        const uint8_t index = _pend.index;
        const uint8_t txn = frame.txn;
        const State st = _state;

        if (st == State::GetAsk) {
            resetLocal();
            if (_sink != nullptr) {
                _sink->onReply(tag, index, view, err);
            }
            return;
        }
        if (st == State::SetIn) {
            resetLocal();
            finishSetIn(tag, index, view, err, txn);
            return;
        }
        if (st == State::SetOut) {
            if (err == MultiFrame::Error::None) {
                beginAck(_now);
                return;
            }
            Nack n{};
            n.error = Err::Body;
            n.detail = static_cast<uint32_t>(err);
            resetLocal();
            if (_sink != nullptr) {
                _sink->onSetAck(tag, index, false, n);
            }
            return;
        }
        if (st == State::GetReply) {
            resetLocal();
        }
    }

    [[nodiscard]] bool submitTx(const IBuf& buf) noexcept
    {
        return submit(buf, FrameJob::Dir::Tx);
    }

    [[nodiscard]] bool submitRx(const IBuf& buf) noexcept
    {
        return submit(buf, FrameJob::Dir::Rx);
    }

    [[nodiscard]] bool submit(const IBuf& buf, FrameJob::Dir dir) noexcept
    {
        if (_job.isBusy() || _pend.txn == 0u) {
            return false;
        }
        _job.buf = buf;
        _job.txn = _pend.txn;
        _job.dir = dir;
        _job.error = MultiFrame::Error::None;
        _job.got = 0;
        _job.owner = this;
        return _xfer.submit(_job);
    }

    SessionT& _session;
    MultiFrame& _xfer;
    ITaggedSink* _sink = nullptr;
    State _state = State::Idle;
    PendSlot _pend{};
    FrameJob _job{};
    IBuf _txBuf{};
    bool _deferTx = false;
    MISC::ReplyTimer _ack;
    Frame _out{};
    bool _outPending = false;
    bool _inFlight = false;
    bool _wireOut = false;
    uint8_t _nextTxn = 1;
    MISC::time_t _now = 0;
};

} // namespace smcp::proto
