/**
 * @file tagged_ops.cpp
 */

#include "smcp/session/tagged_ops.hpp"

#include "smcp/session.hpp"

#include <cstring>

namespace smcp {

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
    _ack.start(now_ms, msg::Ack::kTimeoutMs);
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
            _s.onReply(getTag(), _mb.bufReceived(), MultiByte::Error::Abort);
        } else if (_state == State::SetIn) {
            /* Wire Nack(Abort): писатель в SetOutAwaitAck / mid-TX узнает сразу. */
            finishSetIn(_mb.tag(), _mb.bufReceived(), MultiByte::Error::Abort, _mb.txn());
        } else if (isSetOut()) {
            _s.nack(_pend, msg::Nack::Error::Body,
                    static_cast<uint32_t>(MultiByte::Error::Abort));
        }
    }
    reset();
}

bool TaggedOps::takeSetBuf(uint8_t tag, uint8_t txn_id, uint16_t size,
                           IBuf& buf) noexcept
{
    return _s.onSetBuffer(tag, txn_id, size, buf)
        && buf.size >= size
        && (size == 0u || buf.data != nullptr);
}

void TaggedOps::finishSetIn(uint8_t tag, const IBuf& buf, MultiByte::Error err,
                            uint8_t txn) noexcept
{
    _s.onSet(tag, buf, err);
    if (txn == 0u) {
        return;
    }
    if (err == MultiByte::Error::None) {
        _s.sendAck(txn);
    } else {
        _s.sendNack(txn, msg::Nack::Error::Body, static_cast<uint32_t>(err));
    }
}

bool TaggedOps::sendBody(uint8_t tag, const IBuf& buf, uint8_t txn_id) noexcept
{
    if (!_s.isOpen() || txn_id == 0u || buf.size == 0u || buf.data == nullptr) {
        return false;
    }
    /* Короткий Get reply — на wire (не трогает state / MB). */
    if (buf.size <= msg::kInlineMax) {
        return _s.send(msg::Data::make(buf.data, static_cast<uint8_t>(buf.size)),
                       txn_id);
    }
    /* Длинный — нужен MB; нельзя поверх чужого job. */
    if (isBusy()) {
        return false;
    }
    if (!_mb.armTx(tag, buf, txn_id, this)) {
        return false;
    }
    _state = State::GetReply;
    return true;
}

bool TaggedOps::set(uint8_t tag, const IBuf& buf) noexcept
{
    if (!_s.isOpen() || buf.size == 0u || buf.data == nullptr || isBusy()) {
        return false;
    }
    const uint8_t txn = _s.allocTxn();
    if (txn == 0u) {
        return false;
    }

    /* Короткий Set — на wire. */
    if (buf.size <= msg::kInlineMax) {
        const msg::Set body = msg::Set::make(tag, buf.data,
                                            static_cast<uint8_t>(buf.size));
        Message m{};
        if (!_s.send(body, m, txn)) {
            return false;
        }
        armPend(m, txn);
        _state = State::SetOutInline;
        return true;
    }

    /* Длинный Set — на MB. */
    if (!_mb.armTx(tag, buf, txn, this)) {
        return false;
    }
    const msg::Set body = msg::Set::make(tag);
    Message m{};
    if (!_s.send(body, m, txn)) {
        reset();
        return false;
    }
    armPend(m, txn);
    _state = State::SetOut; /* тело → onMbTxDone → SetOutAwaitAck */
    return true;
}

bool TaggedOps::get(uint8_t tag, IBuf& buf) noexcept
{
    if (!_s.isOpen() || buf.data == nullptr || buf.size == 0u || isBusy()) {
        return false;
    }
    const uint8_t txn = _s.allocTxn();
    if (txn == 0u) {
        return false;
    }
    /* Приём тела сразу. DataBegin забирает MB; короткий Data — finishShortData. */
    if (!_mb.armRx(tag, txn, _s.node.clockMs(), this, buf)) {
        return false;
    }
    _state = State::GetAsk;
    const msg::Get body = msg::Get::make(tag);
    Message m{};
    if (!_s.send(body, m, txn)) {
        reset();
        return false;
    }
    armPend(m, txn);
    return true;
}

uint8_t TaggedOps::getTag() const noexcept
{
    msg::Get body{};
    if (!body.unpack(_pend.msg)) {
        return 0;
    }
    return body.tag;
}

void TaggedOps::failGet(MultiByte::Error err) noexcept
{
    const uint8_t tag = getTag();
    const IBuf got = _mb.bufReceived();
    _s.onReply(tag, got, err);
    reset();
}

void TaggedOps::finishShortData(const msg::Data& chunk) noexcept
{
    if (chunk.seq != 0u) {
        failGet(MultiByte::Error::Seq);
        return;
    }
    if (chunk.len > msg::kInlineMax) {
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
    _s.onReply(getTag(), got, MultiByte::Error::None);
    reset();
}

void TaggedOps::onTxResult(const TxSlot& slot) noexcept
{
    if (_state == State::SetOutInline && slot.txn_id == _pend.txn_id
        && slot.msg.id == msg::Set::kId) {
        beginAck(slot.txn_id, msg::Set::kId, _s.node.clockMs());
    }
}

void TaggedOps::onMbTxDone() noexcept
{
    /* Set ещё ждёт Ack: opener в _pend остаётся, отпускаем только тело. */
    if (_state == State::SetOut) {
        const uint8_t txn = _pend.txn_id;
        _mb.resetXfer();
        beginAck(txn, msg::Set::kId, _s.node.clockMs());
        return;
    }
    reset();
}

void TaggedOps::tick(uint32_t now_ms) noexcept
{
    if (_state == State::SetOutAwaitAck && _ack.timedOut(now_ms)) {
        if (_ack.isReplyLimit()) {
            _s.nack(_pend, msg::Nack::kTimeout);
            reset();
        } else {
            /* Miss: только перезапуск таймера. Re-TX opener/тела нет (у окна A — есть). */
            _ack.start(now_ms, msg::Ack::kTimeoutMs);
        }
    }
}

bool TaggedOps::onMbBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                           IBuf& buf) noexcept
{
    if (!takeSetBuf(tag, txn_id, size, buf)) {
        _s.sendNack(txn_id, msg::Nack::Error::NotSupported);
        reset();
        return false;
    }
    return true;
}

void TaggedOps::onMbResult(uint8_t tag, const IBuf& buf,
                           MultiByte::Error err) noexcept
{
    if (_state == State::GetAsk) {
        _s.onReply(tag, buf, err);
    } else if (_state == State::SetIn) {
        finishSetIn(tag, buf, err, _mb.txn());
    }
    reset();
}

bool TaggedOps::onPacket(const Packet& pkt, uint32_t now_ms) noexcept
{
    if (!_s.isOpen()) {
        return false;
    }

    const uint8_t txn = pkt.txn_id;

    SMCP_IF_MSG(msg::Ack) {
        if (_state != State::SetOutAwaitAck || txn != _pend.txn_id) {
            return false;
        }
        _s.ack(_pend, body);
        reset();
        return true;
    }
    SMCP_IF_MSG(msg::Nack) {
        if (txn != _pend.txn_id) {
            return false;
        }
        if (isSetOut()) {
            _s.nack(_pend, body);
            reset();
            return true;
        }
        if (_state == State::GetAsk) {
            _s.nack(_pend, body);
            failGet(MultiByte::Error::Abort);
            return true;
        }
        return false;
    }
    SMCP_IF_MSG(msg::Data) {
        /* Пока фаза Begin — это короткий ответ. Куски тела (isData) забирает MB. */
        if (_state != State::GetAsk || txn != _pend.txn_id || _mb.isData()) {
            return false;
        }
        finishShortData(body);
        return true;
    }
    SMCP_IF_MSG(msg::Get) {
        /* Busy не здесь: short sendBody ок при job; long режет sendBody → Nack из onGet. */
        _s.onGet(body, txn);
        return true;
    }
    SMCP_IF_MSG(msg::Set) {
        if (isBusy()) {
            _s.sendNack(txn, msg::Nack::Error::Busy);
            return true;
        }
        if (body.len == 0u) {
            if (!_mb.armRx(body.tag, txn, now_ms, this)) {
                _s.sendNack(txn, msg::Nack::Error::Body,
                            static_cast<uint32_t>(MultiByte::Error::NotArmed));
                return true;
            }
            _state = State::SetIn;
            return true;
        }
        //Inline SETIN
        IBuf buf{};
        if (!takeSetBuf(body.tag, txn, body.len, buf)) {
            _s.sendNack(txn, msg::Nack::Error::NotSupported);
            return true;
        }
        std::memcpy(buf.data, body.data, body.len);
        buf.size = body.len;
        finishSetIn(body.tag, buf, MultiByte::Error::None, txn);
        return true;
    }
    return false;
}

} // namespace smcp
