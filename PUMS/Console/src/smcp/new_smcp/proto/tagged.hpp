/**
 * @file new_smcp/proto/tagged.hpp
 * @brief Get / Set opener + короткое тело; длинное — MultiByte.
 *
 * Клиент: get/set → onReply / onSetAck.
 * Сервер: onGet / onSetBuffer+onSet → sendBody / Ack|Nack.
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "comm/container.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/ibuf.hpp"
#include "smcp/new_smcp/packet.hpp"
#include "smcp/new_smcp/pdu.hpp"
#include "smcp/new_smcp/proto/multibyte.hpp"

namespace smcp::proto {

/** Прикладной слой поверх Tagged. */
struct ITaggedSink {
    /** Входящий Get (сервер): ответить sendBody или Nack. */
    virtual void onGet(const Get& body, uint8_t txn) noexcept = 0;

    /**
     * NeedBuf / inline Set: отдать буфер ≥ size.
     * @return false — NotSupported.
     */
    [[nodiscard]] virtual bool onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn,
                                           uint16_t size, IBuf& buf) noexcept = 0;

    /** Тело Set принято (транспорт Ok). Err::None → Ack, иначе Nack. */
    [[nodiscard]] virtual Err onSet(uint8_t tag, uint8_t index,
                                    const IBuf& buf) noexcept = 0;

    /** Итог своего Get (клиент). */
    virtual void onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                         MultiByte::Error err) noexcept = 0;

    /** Итог своего Set (клиент): ok или Nack. */
    virtual void onSetAck(uint8_t tag, uint8_t index, bool ok,
                          const Nack& nack) noexcept = 0;

protected:
    ~ITaggedSink() = default;
};

class Tagged : public comm::Proto<Packet>, private MultiByteClient {
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

    Tagged(SessionT& session, MultiByte& mb, ITaggedSink* sink = nullptr) noexcept
        : Proto(session)
        , _session(session)
        , _mb(mb)
        , _sink(sink)
        , _ack(Ack::kRetry)
    {}

    void setSink(ITaggedSink* sink) noexcept { _sink = sink; }

    [[nodiscard]] bool isBusy() const noexcept { return _state != State::Idle; }
    [[nodiscard]] State state() const noexcept { return _state; }

    [[nodiscard]] Result set(uint8_t tag, const IBuf& buf,
                             uint8_t index = proto::msg::kIndexAll) noexcept
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

        if (buf.size <= proto::msg::kInlineMax) {
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

        if (!_mb.armTx(tag, buf, txn, this, index)) {
            return Result::Fail;
        }
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
                             uint8_t index = proto::msg::kIndexAll) noexcept
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
        if (!_mb.armRx(tag, txn, _now, this, buf, index)) {
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

    /** Ответ на Get: short Data или DataBegin→Data*. */
    [[nodiscard]] bool sendBody(uint8_t tag, const IBuf& buf, uint8_t txn,
                                uint8_t index = proto::msg::kIndexAll) noexcept
    {
        if (!_session.isOpen() || txn == 0u || buf.size == 0u || buf.data == nullptr) {
            return false;
        }
        if (buf.size <= proto::msg::kInlineMax) {
            Data d{};
            d.seq = 0;
            d.len = static_cast<uint8_t>(buf.size);
            std::memcpy(d.data, buf.data, d.len);
            queueOut(txn, [&](proto::msg::Header& m) { d.pack(m); });
            return true;
        }
        if (isBusy()) {
            return false;
        }
        if (!_mb.armTx(tag, buf, txn, this, index)) {
            return false;
        }
        _state = State::GetReply;
        return true;
    }

    void sendAck(uint8_t txn) noexcept
    {
        queueOut(txn, [](proto::msg::Header& m) { Ack{}.pack(m); });
    }

    void sendNack(uint8_t txn, Err err, uint32_t detail = Nack::kDetailNone) noexcept
    {
        Nack n{};
        n.error = err;
        n.detail = detail;
        queueOut(txn, [&](proto::msg::Header& m) { n.pack(m); });
    }

    [[nodiscard]] bool onPacket(const Packet& pkt, MISC::time_t now) noexcept override
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
                failGet(MultiByte::Error::Abort);
                return true;
            }
            return false;
        }
        SMCP_IF_MSG(Data) {
            if (_state != State::GetAsk || txn != _pend.txn || _mb.isData()) {
                return false;
            }
            finishShortData(body);
            return true;
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
                if (!_mb.armRx(body.tag, txn, now, this, {}, body.index)) {
                    sendNack(txn, Err::Body,
                             static_cast<uint32_t>(MultiByte::Error::NotArmed));
                    return true;
                }
                _state = State::SetIn;
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
            finishSetIn(body.tag, body.index, buf, MultiByte::Error::None, txn);
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
        _mb.abort();
        if (_state == State::GetAsk) {
            failGet(MultiByte::Error::Abort);
            return;
        }
        if (_state == State::SetIn) {
            finishSetIn(_mb.tag(), _mb.index(), _mb.bufReceived(),
                        MultiByte::Error::Abort, _mb.txn());
            reset();
            return;
        }
        if (isSetOut()) {
            Nack n{};
            n.error = Err::Body;
            n.detail = static_cast<uint32_t>(MultiByte::Error::Abort);
            const uint8_t tag = _pend.tag;
            const uint8_t index = _pend.index;
            reset();
            if (_sink != nullptr) {
                _sink->onSetAck(tag, index, false, n);
            }
            return;
        }
        reset();
    }

protected:
    [[nodiscard]] bool peekTx(Packet& out, MISC::time_t now) noexcept override
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
            out.msg = _pend.msg;
            _inFlight = true;
            _wireOut = false;
            return true;
        }
        return false;
    }

    void onTxDone(const Packet&, bool ok) noexcept override
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
                failGet(MultiByte::Error::Abort);
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
        }
        // GetAsk / SetOut: opener ушёл; ждём Data / MB / Ack после тела
    }

private:
    enum class Pend : uint8_t { None = 0, GetOpener, SetOpener };

    struct PendSlot {
        Pend kind = Pend::None;
        uint8_t txn = 0;
        uint8_t tag = 0;
        uint8_t index = 0;
        bool sent = false;
        proto::msg::Header msg{};
    };

    [[nodiscard]] bool isSetOut() const noexcept
    {
        return _state == State::SetOutInline || _state == State::SetOut
            || _state == State::SetOutAwaitAck;
    }

    void reset() noexcept
    {
        _mb.resetXfer();
        _state = State::Idle;
        _pend = {};
        _ack.clear();
        _outPending = false;
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

    void failGet(MultiByte::Error err) noexcept
    {
        readOpenerTag();
        const uint8_t tag = _pend.tag;
        const uint8_t index = _pend.index;
        const IBuf got = _mb.bufReceived();
        reset();
        if (_sink != nullptr) {
            _sink->onReply(tag, index, got, err);
        }
    }

    void finishShortData(const Data& chunk) noexcept
    {
        if (chunk.seq != 0u) {
            failGet(MultiByte::Error::Seq);
            return;
        }
        const IBuf dst = _mb.bufReserved();
        if (chunk.len > dst.size) {
            failGet(MultiByte::Error::Overflow);
            return;
        }
        if (chunk.len > 0u && dst.data != nullptr) {
            std::memcpy(dst.data, chunk.data, chunk.len);
        }
        readOpenerTag();
        const uint8_t tag = _pend.tag;
        const uint8_t index = _pend.index;
        const IBuf got{dst.data, chunk.len};
        reset();
        if (_sink != nullptr) {
            _sink->onReply(tag, index, got, MultiByte::Error::None);
        }
    }

    void finishSetIn(uint8_t tag, uint8_t index, const IBuf& buf,
                     MultiByte::Error err, uint8_t txn) noexcept
    {
        if (err != MultiByte::Error::None) {
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
        packMsg(_out.msg);
        _outPending = true;
    }

    [[nodiscard]] uint8_t allocTxn() noexcept
    {
        for (uint8_t n = 0; n < proto::msg::kTxnIdMax; ++n) {
            uint8_t t = _nextTxn++;
            if (_nextTxn == 0u || _nextTxn > proto::msg::kTxnIdMax) {
                _nextTxn = 1u;
            }
            if (t != 0u) {
                return t;
            }
        }
        return 0;
    }

    // MultiByteClient
    [[nodiscard]] bool onMbBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                                  uint16_t size, IBuf& buf) noexcept override
    {
        if (_sink == nullptr
            || !_sink->onSetBuffer(tag, index, txn_id, size, buf)
            || buf.size < size || (size > 0u && buf.data == nullptr)) {
            sendNack(txn_id, Err::NotSupported);
            reset();
            return false;
        }
        return true;
    }

    void onMbResult(uint8_t tag, uint8_t index, const IBuf& buf,
                    MultiByte::Error err) noexcept override
    {
        if (_state == State::GetAsk) {
            reset();
            if (_sink != nullptr) {
                _sink->onReply(tag, index, buf, err);
            }
            return;
        }
        if (_state == State::SetIn) {
            const uint8_t txn = _mb.txn();
            finishSetIn(tag, index, buf, err, txn);
            reset();
            return;
        }
        if (_state == State::GetReply || _state == State::SetOut) {
            // TX fail mid-body
            if (_state == State::SetOut) {
                Nack n{};
                n.error = Err::Body;
                n.detail = static_cast<uint32_t>(err);
                const uint8_t t = _pend.tag;
                const uint8_t i = _pend.index;
                readOpenerTag();
                reset();
                if (_sink != nullptr) {
                    _sink->onSetAck(t != 0u ? t : tag, i != 0u ? i : index, false, n);
                }
            } else {
                reset();
            }
        }
    }

    void onMbTxDone() noexcept override
    {
        if (_state == State::SetOut) {
            readOpenerTag();
            _mb.resetXfer();
            beginAck(_now);
            return;
        }
        // GetReply
        reset();
    }

    SessionT& _session;
    MultiByte& _mb;
    ITaggedSink* _sink = nullptr;
    State _state = State::Idle;
    PendSlot _pend{};
    MISC::ReplyTimer _ack;
    Packet _out{};
    bool _outPending = false;
    bool _inFlight = false;
    bool _wireOut = false;
    uint8_t _nextTxn = 1;
    MISC::time_t _now = 0;
};

} // namespace smcp::proto
