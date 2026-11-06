/**
 * @file tagged_ops.cpp
 */

#include "smcp/session/tagged_ops.hpp"

#include "smcp/session.hpp"

#include <cstring>

namespace smcp::session {

TaggedOps::TaggedOps(Session& session, MultiByte& mb) noexcept
    : _s(session)
    , _mb(mb)
{}

void TaggedOps::armPend(const Message& m, uint8_t txn) noexcept
{
    _pend = {};
    _pend.msg = m;
    _pend.txn_id = txn;
    _pend.dst_id = _s.peerId();
}

void TaggedOps::beginAck(uint8_t txn, uint8_t msg_id, uint32_t now_ms) noexcept
{
    _state = State::SetOutAwaitAck;
    _pend.txn_id = txn;
    _pend.msg.id = msg_id;
    _pend.dst_id = _s.peerId();
    _ack.start(now_ms, Ack::kTimeoutMs);
}

void TaggedOps::reset() noexcept
{
    _mb.resetXfer();
    _state = State::Idle;
    _pend = {};
    _ack.clear();
}

void TaggedOps::abort(bool notify) noexcept
{
    _mb.abort();
    if (notify) {
        if (_state == State::GetAsk) {
            uint8_t tag = 0;
            uint8_t index = 0;
            readOpener(tag, index);
            _s.onReply(tag, index, _mb.bufReceived(), MultiByte::Error::Abort);
        } else if (_state == State::SetIn) {
            /* Wire Nack(Abort): писатель в SetOutAwaitAck / mid-TX узнает сразу. */
            finishSetIn(_mb.tag(), _mb.index(), _mb.bufReceived(),
                        MultiByte::Error::Abort, _mb.txn());
        } else if (isSetOut()) {
            _s.nack(_pend, Error::Body,
                    static_cast<uint32_t>(MultiByte::Error::Abort));
        }
    }
    reset();
}

bool TaggedOps::takeSetBuf(uint8_t tag, uint8_t index, uint8_t txn_id,
                           uint16_t size, IBuf& buf) noexcept
{
    return _s.onSetBuffer(tag, index, txn_id, size, buf)
        && buf.size >= size
        && (size == 0u || buf.data != nullptr);
}

void TaggedOps::finishSetIn(uint8_t tag, uint8_t index, const IBuf& buf,
                            MultiByte::Error err, uint8_t txn) noexcept
{
    if (err != MultiByte::Error::None) {
        if (txn != 0u) {
            _s.sendNack(txn, Error::Body, static_cast<uint32_t>(err));
        }
        return;
    }
    const vError code = _s.onSet(tag, index, buf);
    if (txn == 0u) {
        return;
    }
    if (code == Error::None) {
        _s.sendAck(txn);
    } else {
        _s.sendNack(txn, code);
    }
}

bool TaggedOps::sendBody(uint8_t tag, const IBuf& buf, uint8_t txn_id,
                         uint8_t index) noexcept
{
    if (!_s.isOpen() || txn_id == 0u || buf.size == 0u || buf.data == nullptr) {
        return false;
    }
    /* Короткий Get reply — на wire (не трогает state / MB). */
    if (buf.size <= kInlineMax) {
        return _s.send(Data::make(buf.data, static_cast<uint8_t>(buf.size)),
                       txn_id);
    }
    /* Длинный — нужен MB; нельзя поверх чужого job. */
    if (isBusy()) {
        return false;
    }
    if (!_mb.armTx(tag, buf, txn_id, this, index)) {
        return false;
    }
    _state = State::GetReply;
    return true;
}

const char* TaggedOps::cstr(Result s) noexcept
{
    switch (s) {
    case Result::Ok: return "Ok";
    case Result::Busy: return "Busy";
    case Result::Fail: return "Fail";
    }
    return "?";
}

TaggedOps::Result TaggedOps::set(uint8_t tag, const IBuf& buf, uint8_t index) noexcept
{
    if (!_s.isOpen() || buf.size == 0u || buf.data == nullptr) {
        return Result::Fail;
    }
    if (isBusy()) {
        return Result::Busy;
    }
    const uint8_t txn = _s.allocTxn();
    if (txn == 0u) {
        return Result::Fail;
    }

    /* Короткий Set — на wire. */
    if (buf.size <= kInlineMax) {
        const Set body = Set::make(tag, buf.data,
                                            static_cast<uint8_t>(buf.size), index);
        Message m{};
        if (!_s.send(body, m, txn)) {
            return Result::Fail;
        }
        armPend(m, txn);
        _state = State::SetOutInline;
        return Result::Ok;
    }

    /* Длинный Set — на MB. */
    if (!_mb.armTx(tag, buf, txn, this, index)) {
        return Result::Fail;
    }
    const Set body = Set::make(tag, index);
    Message m{};
    if (!_s.send(body, m, txn)) {
        reset();
        return Result::Fail;
    }
    armPend(m, txn);
    _state = State::SetOut; /* тело → onMbTxDone → SetOutAwaitAck */
    return Result::Ok;
}

TaggedOps::Result TaggedOps::get(uint8_t tag, IBuf& buf, uint8_t index) noexcept
{
    if (!_s.isOpen() || buf.data == nullptr || buf.size == 0u) {
        return Result::Fail;
    }
    if (isBusy()) {
        return Result::Busy;
    }
    const uint8_t txn = _s.allocTxn();
    if (txn == 0u) {
        return Result::Fail;
    }
    /* Приём тела сразу. DataBegin забирает MB; короткий Data — finishShortData. */
    if (!_mb.armRx(tag, txn, _s.node.clockMs(), this, buf, index)) {
        return Result::Fail;
    }
    _state = State::GetAsk;
    const Get body = Get::make(tag, index);
    Message m{};
    if (!_s.send(body, m, txn)) {
        reset();
        return Result::Fail;
    }
    armPend(m, txn);
    return Result::Ok;
}

void TaggedOps::readOpener(uint8_t& tag, uint8_t& index) const noexcept
{
    Get body{};
    if (!body.unpack(_pend.msg)) {
        tag = 0;
        index = 0;
        return;
    }
    tag = body.tag;
    index = body.index;
}

void TaggedOps::failGet(MultiByte::Error err) noexcept
{
    uint8_t tag = 0;
    uint8_t index = 0;
    readOpener(tag, index);
    const IBuf got = _mb.bufReceived();
    _s.onReply(tag, index, got, err);
    reset();
}

void TaggedOps::finishShortData(const Data& chunk) noexcept
{
    if (chunk.seq != 0u) {
        failGet(MultiByte::Error::Seq);
        return;
    }
    if (chunk.len > kInlineMax) {
        failGet(MultiByte::Error::Unexpected);
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
    const IBuf got{dst.data, chunk.len};
    uint8_t tag = 0;
    uint8_t index = 0;
    readOpener(tag, index);
    _s.onReply(tag, index, got, MultiByte::Error::None);
    reset();
}

void TaggedOps::onTxResult(const TxSlot& slot) noexcept
{
    if (_state == State::SetOutInline && slot.txn_id == _pend.txn_id
        && slot.msg.id == Set::kId) {
        beginAck(slot.txn_id, Set::kId, _s.node.clockMs());
    }
}

void TaggedOps::onMbTxDone() noexcept
{
    /* Set ещё ждёт Ack: opener в _pend остаётся, отпускаем только тело. */
    if (_state == State::SetOut) {
        const uint8_t txn = _pend.txn_id;
        _mb.resetXfer();
        beginAck(txn, Set::kId, _s.node.clockMs());
        return;
    }
    reset();
}

void TaggedOps::tick(uint32_t now_ms) noexcept
{
    if (_state == State::SetOutAwaitAck && _ack.timedOut(now_ms)) {
        if (_ack.isReplyLimit()) {
            _s.nack(_pend, Error::Timeout);
            reset();
        } else {
            /* Miss: только перезапуск таймера. Re-TX opener/тела нет (у окна A — есть). */
            _ack.start(now_ms, Ack::kTimeoutMs);
        }
    }
}

bool TaggedOps::onMbBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                           uint16_t size, IBuf& buf) noexcept
{
    if (!takeSetBuf(tag, index, txn_id, size, buf)) {
        _s.sendNack(txn_id, Error::NotSupported);
        reset();
        return false;
    }
    return true;
}

void TaggedOps::onMbResult(uint8_t tag, uint8_t index, const IBuf& buf,
                           MultiByte::Error err) noexcept
{
    if (_state == State::GetAsk) {
        _s.onReply(tag, index, buf, err);
    } else if (_state == State::SetIn) {
        finishSetIn(tag, index, buf, err, _mb.txn());
    }
    reset();
}

bool TaggedOps::onPacket(const Packet& pkt, uint32_t now_ms) noexcept
{
    if (!_s.isOpen()) {
        return false;
    }

    const uint8_t txn = pkt.txn_id;

    SMCP_IF_MSG(Ack) {
        if (_state != State::SetOutAwaitAck || txn != _pend.txn_id) {
            return false;
        }
        /* reset до ack: onAck может стартовать новый Get/Set (напр. AxisRegistered). */
        const TxSlot pend = _pend;
        reset();
        _s.ack(pend, body);
        return true;
    }
    SMCP_IF_MSG(Nack) {
        if (txn != _pend.txn_id) {
            return false;
        }
        if (isSetOut()) {
            const TxSlot pend = _pend;
            reset();
            _s.nack(pend, body);
            return true;
        }
        if (_state == State::GetAsk) {
            _s.nack(_pend, body);
            failGet(MultiByte::Error::Abort);
            return true;
        }
        return false;
    }
    SMCP_IF_MSG(Data) {
        /* Пока фаза Begin — это короткий ответ. Куски тела (isData) забирает MB. */
        if (_state != State::GetAsk || txn != _pend.txn_id || _mb.isData()) {
            return false;
        }
        finishShortData(body);
        return true;
    }
    SMCP_IF_MSG(Get) {
        /* Busy не здесь: short sendBody ок при job; long режет sendBody → Nack из onGet. */
        _s.onGet(body, txn);
        return true;
    }
    SMCP_IF_MSG(Set) {
        if (isBusy()) {
            _s.sendNack(txn, Error::Busy);
            return true;
        }
        if (body.len == 0u) {
            if (!_mb.armRx(body.tag, txn, now_ms, this, {}, body.index)) {
                _s.sendNack(txn, Error::Body,
                            static_cast<uint32_t>(MultiByte::Error::NotArmed));
                return true;
            }
            _state = State::SetIn;
            return true;
        }
        //Inline SETIN
        IBuf buf{};
        if (!takeSetBuf(body.tag, body.index, txn, body.len, buf)) {
            _s.sendNack(txn, Error::NotSupported);
            return true;
        }
        std::memcpy(buf.data, body.data, body.len);
        buf.size = body.len;
        finishSetIn(body.tag, body.index, buf, MultiByte::Error::None, txn);
        return true;
    }
    return false;
}

} // namespace smcp::session
