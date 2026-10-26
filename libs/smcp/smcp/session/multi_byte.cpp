/**
 * @file multi_byte.cpp
 */

#include "smcp/session/multi_byte.hpp"

#include "smcp/session.hpp"

#include "crc.hpp"

#include <cstring>

namespace smcp {

MultiByte::MultiByte(Session& session) noexcept
    : _s(session)
{}

// --- types ---

const char* MultiByte::cstr(Error err) noexcept
{
    switch (err) {
    case Error::None: return "Ok";
    case Error::NotArmed: return "NotArmed";
    case Error::TagMismatch: return "TagMismatch";
    case Error::Seq: return "Seq";
    case Error::Overflow: return "Overflow";
    case Error::Crc: return "Crc";
    case Error::Unexpected: return "Unexpected";
    case Error::Abort: return "Abort";
    case Error::Timeout: return "Timeout";
    }
    return "?";
}

// --- arm ---

bool MultiByte::armRx(uint8_t tag, uint8_t txn_id, uint32_t now_ms,
                      MultiByteClient* client, const IBuf& buf) noexcept
{
    return arm(tag, buf, false, txn_id, now_ms, client);
}

bool MultiByte::armTx(uint8_t tag, const IBuf& buf, uint8_t txn_id,
                      MultiByteClient* client) noexcept
{
    return arm(tag, buf, true, txn_id, 0u, client);
}

bool MultiByte::arm(uint8_t tag, const IBuf& buf, bool tx, uint8_t txn_id,
                    uint32_t now_ms, MultiByteClient* client) noexcept
{
    if (isBusy() || txn_id == 0u || client == nullptr) {
        return false;
    }
    if (tx) {
        /* size≥1: пустой blob / DataBegin(size=0) запрещены. */
        if (buf.data == nullptr || buf.size == 0u) {
            return false;
        }
    } else if ((buf.data == nullptr) != (buf.size == 0u)) {
        return false;
    }
    _mode = tx ? Mode::Tx : Mode::Rx;
    _phase = Phase::Begin;
    _error = Error::None;
    _finished = false;
    _tag = tag;
    _txn = txn_id;
    _client = client;
    _buf = buf;
    _xfer = {};
    if (tx) {
        _xfer.size = buf.size;
        _xfer.crc = MISC::Crc32::compute(buf.data, buf.size);
    } else {
        _watch.start(now_ms, msg::DataBegin::kTimeoutMs);
    }
    return true;
}

bool MultiByte::attachRx(const IBuf& buf, uint16_t need) noexcept
{
    if (_mode != Mode::Rx || _finished || _phase != Phase::Begin || _buf.data != nullptr) {
        return false;
    }
    if (buf.size < need || (need > 0u && buf.data == nullptr)) {
        return false;
    }
    _buf = buf;
    return true;
}

// --- TX stream ---

bool MultiByte::peekTx(Message& out) const noexcept
{
    if (_mode != Mode::Tx || _finished) {
        return false;
    }
    switch (_phase) {
    case Phase::Begin: {
        return msg::DataBegin::make(_tag, _xfer.size, _xfer.crc).pack(out);
    }
    case Phase::Data: {
        const uint8_t n = txChunkLen();
        const uint8_t* src = (n > 0u && _buf.data != nullptr)
            ? _buf.data + _xfer.offset : nullptr;
        return msg::Data::make(_xfer.seq, src, n).pack(out);
    }
    }
    return false;
}

bool MultiByte::onTxResult(const TxSlot& slot) noexcept
{
    const uint8_t id = slot.msg.id;
    if (!_tx_pend || !isTx() || _finished || _txn == 0u || slot.txn_id != _txn
        || (id != msg::DataBegin::kId && id != msg::Data::kId)) {
        return false;
    }
    _tx_pend = false;
    switch (_phase) {
    case Phase::Begin:
        _phase = Phase::Data;
        break;
    case Phase::Data: {
        const uint8_t n = txChunkLen();
        const uint32_t next = static_cast<uint32_t>(_xfer.offset) + n;
        if (next > _xfer.size) {
            break; /* txChunkLen не должен так дать; не двигаем offset */
        }
        _xfer.offset = static_cast<uint16_t>(next);
        _xfer.seq = static_cast<uint8_t>(_xfer.seq + 1u);
        if (_xfer.offset >= _xfer.size) {
            succeed();
            if (MultiByteClient* c = _client) {
                c->onMbTxDone();
            }
        }
        break;
    }
    }
    return true;
}

// --- lifecycle ---

void MultiByte::resetXfer() noexcept
{
    _watch.stop();
    _mode = Mode::Idle;
    _phase = Phase::Begin;
    _error = Error::None;
    _finished = false;
    _tx_pend = false;
    _txn = 0;
    _tag = 0;
    _client = nullptr;
    _buf = {};
    _xfer = {};
}

void MultiByte::abort() noexcept
{
    if (!isBusy() || _finished) {
        if (!isBusy()) {
            resetXfer();
        }
        return;
    }
    fail(Error::Abort);
}

// --- pump / RX feed ---

void MultiByte::tick(uint32_t now_ms) noexcept
{
    if (!_finished && _mode == Mode::Rx && _watch.timedOut(now_ms)) {
        fail(Error::Timeout);
        notifyResult();
        return;
    }
    if (_tx_pend || !isTx() || _finished) {
        return;
    }
    Message m{};
    if (peekTx(m) && _s.send(m, _txn)) {
        _tx_pend = true;
    }
}

bool MultiByte::onPacket(const Packet& pkt, uint32_t now_ms) noexcept
{
    if (!isRx() || _finished || _txn == 0u || pkt.txn_id != _txn) {
        return false;
    }

    Result o = Result::Continue;
    msg::DataBegin begin{};
    SMCP_IF_MSG(msg::DataBegin) {
        begin = body;
        o = onBegin(body, now_ms);
    } else SMCP_IF_MSG(msg::Data) {
        /* До DataBegin кадр не наш: короткий Get забирает TaggedOps. */
        if (_phase != Phase::Data) {
            return false;
        }
        o = onData(body, now_ms);
    } else {
        return false;
    }

    if (o == Result::NeedBuf) {
        IBuf buf{};
        if (_client == nullptr) {
            fail(Error::NotArmed);
            return true;
        }
        if (!_client->onMbBuffer(_tag, _txn, begin.size, buf)) {
            /* Клиент уже завершил xfer (Nack или onReply) и reset. */
            return true;
        }
        if (!attachRx(buf, begin.size)) {
            fail(Error::NotArmed);
            notifyResult();
            return true;
        }
        o = enterData(begin, now_ms);
    }

    if (o == Result::Done || o == Result::Fail) {
        notifyResult();
    }
    return true;
}

MultiByte::Result MultiByte::onBegin(const msg::DataBegin& begin,
                                     uint32_t now_ms) noexcept
{
    if (_finished) {
        return finishedResult();
    }
    if (_mode != Mode::Rx) {
        fail(Error::NotArmed);
        return Result::Fail;
    }
    if (begin.size == 0u) {
        fail(Error::Unexpected);
        return Result::Fail;
    }
    if (_phase != Phase::Begin) {
        fail(Error::Unexpected);
        return Result::Fail;
    }
    if (begin.tag != _tag) {
        fail(Error::TagMismatch);
        return Result::Fail;
    }
    if (_buf.data == nullptr) {
        return Result::NeedBuf;
    }
    return enterData(begin, now_ms);
}

MultiByte::Result MultiByte::enterData(const msg::DataBegin& begin,
                                       uint32_t now_ms) noexcept
{
    if (_buf.data == nullptr) {
        fail(Error::NotArmed);
        return Result::Fail;
    }
    if (begin.size > _buf.size) {
        fail(Error::Overflow);
        return Result::Fail;
    }
    _xfer = {begin.size, begin.crc32, 0u, 0u};
    _phase = Phase::Data;
    _watch.start(now_ms, msg::DataBegin::kTimeoutMs);
    return Result::Continue;
}

MultiByte::Result MultiByte::onData(const msg::Data& chunk, uint32_t now_ms) noexcept
{
    if (_finished) {
        return finishedResult();
    }
    if (_mode != Mode::Rx) {
        fail(Error::Unexpected);
        return Result::Fail;
    }
    if (chunk.len > msg::kInlineMax) {
        fail(Error::Unexpected);
        return Result::Fail;
    }
    if (chunk.seq != _xfer.seq) {
        fail(Error::Seq);
        return Result::Fail;
    }
    const uint32_t next = static_cast<uint32_t>(_xfer.offset) + chunk.len;
    if (next > _xfer.size) {
        fail(Error::Overflow);
        return Result::Fail;
    }
    if (chunk.len > 0u && _buf.data != nullptr) {
        std::memcpy(_buf.data + _xfer.offset, chunk.data, chunk.len);
    }
    _xfer.offset = static_cast<uint16_t>(next);
    _xfer.seq = static_cast<uint8_t>(_xfer.seq + 1u);
    if (_xfer.offset >= _xfer.size) {
        return finishRxCrc();
    }
    _watch.start(now_ms, msg::DataBegin::kTimeoutMs);
    return Result::Continue;
}

// --- helpers (private) ---

void MultiByte::notifyResult() noexcept
{
    if (_client == nullptr) {
        return;
    }
    MultiByteClient* c = _client;
    c->onMbResult(_tag, bufReceived(), _error);
}

void MultiByte::fail(Error err) noexcept
{
    _watch.stop();
    _error = err;
    _finished = true;
}

void MultiByte::succeed() noexcept
{
    _watch.stop();
    _error = Error::None;
    _finished = true;
}

MultiByte::Result MultiByte::finishRxCrc() noexcept
{
    const uint32_t crc = (_xfer.size > 0u && _buf.data != nullptr)
        ? MISC::Crc32::compute(_buf.data, _xfer.size)
        : 0u;
    if (crc != _xfer.crc) {
        fail(Error::Crc);
        return Result::Fail;
    }
    succeed();
    return Result::Done;
}

MultiByte::Result MultiByte::finishedResult() const noexcept
{
    return (_error == Error::None) ? Result::Done : Result::Fail;
}

uint8_t MultiByte::txChunkLen() const noexcept
{
    if (_xfer.offset >= _xfer.size) {
        return 0u;
    }
    const uint16_t left = static_cast<uint16_t>(_xfer.size - _xfer.offset);
    return (left > msg::kInlineMax) ? msg::kInlineMax : static_cast<uint8_t>(left);
}

} // namespace smcp
