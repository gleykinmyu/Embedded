/**
 * @file new_smcp/proto/multibyte.hpp
 * @brief Multi-byte тело txn: RX/TX DataBegin → Data* (size + CRC).
 *
 * Короткий ответ Get (один Data) — Tagged, сюда не приходит.
 * Владелец xfer — MultiByteClient (arm хранит указатель).
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "comm/container.hpp"
#include "crc.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/ibuf.hpp"
#include "smcp/new_smcp/session.hpp"

namespace smcp::proto {

struct MultiByteClient;

class MultiByte : public comm::Proto<Packet> {
public:
    enum class Mode : uint8_t { Idle = 0, Rx, Tx };

    enum class Error : uint8_t {
        First = 0,
        None = First,
        NotArmed,
        TagMismatch,
        Seq,
        Overflow,
        Crc,
        Unexpected,
        Abort,
        Timeout,
        Last,
    };

    enum class Result : uint8_t {
        Continue = 0,
        NeedBuf,
        Done,
        Fail,
    };

    explicit MultiByte(Session& session) noexcept
        : Proto(session)
        , _session(session)
    {}

    [[nodiscard]] static const char* cstr(Error err) noexcept
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
        case Error::Last: break;
        }
        return "?";
    }

    [[nodiscard]] Mode mode() const noexcept { return _mode; }
    [[nodiscard]] Error error() const noexcept { return _error; }
    [[nodiscard]] uint8_t tag() const noexcept { return _tag; }
    [[nodiscard]] uint8_t index() const noexcept { return _index; }
    [[nodiscard]] uint8_t txn() const noexcept { return _txn; }
    [[nodiscard]] IBuf bufReceived() const noexcept { return {_buf.data, _xfer.offset}; }
    [[nodiscard]] IBuf bufReserved() const noexcept { return _buf; }
    [[nodiscard]] bool isBusy() const noexcept { return _mode != Mode::Idle; }
    [[nodiscard]] bool isRx() const noexcept { return _mode == Mode::Rx; }
    [[nodiscard]] bool isTx() const noexcept { return _mode == Mode::Tx; }
    [[nodiscard]] bool isData() const noexcept { return _phase == Phase::Data; }
    [[nodiscard]] bool isFinished() const noexcept { return _finished; }

    [[nodiscard]] bool armRx(uint8_t tag, uint8_t txn_id, MISC::time_t now,
                             MultiByteClient* client, const IBuf& buf = {},
                             uint8_t index = proto::msg::kIndexAll) noexcept
    {
        return arm(tag, index, buf, false, txn_id, now, client);
    }

    [[nodiscard]] bool attachRx(const IBuf& buf, uint16_t need) noexcept
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

    [[nodiscard]] bool armTx(uint8_t tag, const IBuf& buf, uint8_t txn_id,
                             MultiByteClient* client,
                             uint8_t index = proto::msg::kIndexAll) noexcept
    {
        return arm(tag, index, buf, true, txn_id, 0u, client);
    }

    void resetXfer() noexcept
    {
        _watch.stop();
        _mode = Mode::Idle;
        _phase = Phase::Begin;
        _error = Error::None;
        _finished = false;
        _inFlight = false;
        _txn = 0;
        _tag = 0;
        _index = 0;
        _client = nullptr;
        _buf = {};
        _xfer = {};
    }

    void abort() noexcept override
    {
        if (!isBusy() || _finished) {
            if (!isBusy()) {
                resetXfer();
            }
            return;
        }
        fail(Error::Abort);
    }

    [[nodiscard]] bool onPacket(const Packet& pkt, MISC::time_t now) noexcept override;

    void tick(MISC::time_t now) noexcept override
    {
        if (!_finished && _mode == Mode::Rx && _watch.timedOut(now)) {
            fail(Error::Timeout);
            notifyResult();
        }
    }

protected:
    [[nodiscard]] bool peekTx(Packet& out, MISC::time_t) noexcept override
    {
        if (_inFlight || !isTx() || _finished || !_session.isOpen()) {
            return false;
        }
        out = {};
        out.dst = _session.peer();
        out.txn = _txn;
        switch (_phase) {
        case Phase::Begin: {
            DataBegin b{};
            b.tag = _tag;
            b.index = _index;
            b.size = _xfer.size;
            b.crc32 = _xfer.crc;
            b.pack(out.msg);
            break;
        }
        case Phase::Data: {
            const uint8_t n = txChunkLen();
            Data d{};
            d.seq = _xfer.seq;
            d.len = n;
            if (n > 0u && _buf.data != nullptr) {
                std::memcpy(d.data, _buf.data + _xfer.offset, n);
            }
            d.pack(out.msg);
            break;
        }
        }
        _inFlight = true;
        return true;
    }

    void onTxDone(const Packet& frame, bool ok) noexcept override;

private:
    enum class Phase : uint8_t { Begin = 0, Data };

    [[nodiscard]] bool arm(uint8_t tag, uint8_t index, const IBuf& buf, bool tx,
                           uint8_t txn_id, MISC::time_t now, MultiByteClient* client) noexcept
    {
        if (isBusy() || txn_id == 0u || client == nullptr) {
            return false;
        }
        if (tx) {
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
        _inFlight = false;
        _tag = tag;
        _index = index;
        _txn = txn_id;
        _client = client;
        _buf = buf;
        _xfer = {};
        if (tx) {
            _xfer.size = buf.size;
            _xfer.crc = MISC::Crc32::compute(buf.data, buf.size);
        } else {
            _watch.start(now, DataBegin::kTimeoutMs);
        }
        return true;
    }

    [[nodiscard]] Result onBegin(const DataBegin& begin, MISC::time_t now) noexcept
    {
        if (_finished) {
            return finishedResult();
        }
        if (_mode != Mode::Rx) {
            fail(Error::NotArmed);
            return Result::Fail;
        }
        if (begin.size == 0u || _phase != Phase::Begin) {
            fail(Error::Unexpected);
            return Result::Fail;
        }
        if (begin.tag != _tag || begin.index != _index) {
            fail(Error::TagMismatch);
            return Result::Fail;
        }
        if (_buf.data == nullptr) {
            return Result::NeedBuf;
        }
        return enterData(begin, now);
    }

    [[nodiscard]] Result enterData(const DataBegin& begin, MISC::time_t now) noexcept
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
        _watch.start(now, DataBegin::kTimeoutMs);
        return Result::Continue;
    }

    [[nodiscard]] Result onData(const Data& chunk, MISC::time_t now) noexcept
    {
        if (_finished) {
            return finishedResult();
        }
        if (_mode != Mode::Rx) {
            fail(Error::Unexpected);
            return Result::Fail;
        }
        if (chunk.len > proto::msg::kInlineMax) {
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
        _xfer.seq = static_cast<uint16_t>(_xfer.seq + 1u);
        if (_xfer.offset >= _xfer.size) {
            return finishRxCrc();
        }
        _watch.start(now, DataBegin::kTimeoutMs);
        return Result::Continue;
    }

    void notifyResult() noexcept;

    void fail(Error err) noexcept
    {
        _watch.stop();
        _error = err;
        _finished = true;
    }

    void succeed() noexcept
    {
        _watch.stop();
        _error = Error::None;
        _finished = true;
    }

    [[nodiscard]] Result finishRxCrc() noexcept
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

    [[nodiscard]] Result finishedResult() const noexcept
    {
        return (_error == Error::None) ? Result::Done : Result::Fail;
    }

    [[nodiscard]] uint8_t txChunkLen() const noexcept
    {
        if (_xfer.offset >= _xfer.size) {
            return 0u;
        }
        const uint16_t left = static_cast<uint16_t>(_xfer.size - _xfer.offset);
        return (left > proto::msg::kInlineMax) ? proto::msg::kInlineMax : static_cast<uint8_t>(left);
    }

    Session& _session;
    Mode _mode = Mode::Idle;
    Phase _phase = Phase::Begin;
    Error _error = Error::None;
    bool _finished = false;
    bool _inFlight = false;
    uint8_t _tag = 0;
    uint8_t _index = 0;
    uint8_t _txn = 0;
    MultiByteClient* _client = nullptr;
    MISC::MsTimer _watch{};
    IBuf _buf{};
    struct {
        uint16_t size = 0;
        uint32_t crc = 0;
        uint16_t seq = 0;
        uint16_t offset = 0;
    } _xfer{};
};

struct MultiByteClient {
    virtual ~MultiByteClient() = default;

    [[nodiscard]] virtual bool onMbBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                                          uint16_t size, IBuf& buf) noexcept = 0;
    virtual void onMbResult(uint8_t tag, uint8_t index, const IBuf& buf,
                            MultiByte::Error err) noexcept = 0;
    virtual void onMbTxDone() noexcept = 0;
};

inline bool MultiByte::onPacket(const Packet& pkt, MISC::time_t now) noexcept
{
    if (!isRx() || _finished || _txn == 0u || pkt.txn != _txn) {
        return false;
    }

    Result o = Result::Continue;
    DataBegin begin{};
    SMCP_IF_MSG(DataBegin) {
        begin = body;
        o = onBegin(body, now);
    } else SMCP_IF_MSG(Data) {
        if (_phase != Phase::Data) {
            return false;
        }
        o = onData(body, now);
    } else {
        return false;
    }

    if (o == Result::NeedBuf) {
        IBuf buf{};
        if (_client == nullptr) {
            fail(Error::NotArmed);
            return true;
        }
        if (!_client->onMbBuffer(_tag, _index, _txn, begin.size, buf)) {
            return true;
        }
        if (!attachRx(buf, begin.size)) {
            fail(Error::NotArmed);
            notifyResult();
            return true;
        }
        o = enterData(begin, now);
    }

    if (o == Result::Done || o == Result::Fail) {
        notifyResult();
    }
    return true;
}

inline void MultiByte::onTxDone(const Packet& frame, bool ok) noexcept
{
    if (!_inFlight || !isTx() || _finished || frame.txn != _txn) {
        return;
    }
    _inFlight = false;
    if (!ok) {
        fail(Error::Abort);
        notifyResult();
        return;
    }
    switch (_phase) {
    case Phase::Begin:
        _phase = Phase::Data;
        break;
    case Phase::Data: {
        const uint8_t n = txChunkLen();
        const uint32_t next = static_cast<uint32_t>(_xfer.offset) + n;
        if (next > _xfer.size) {
            break;
        }
        _xfer.offset = static_cast<uint16_t>(next);
        _xfer.seq = static_cast<uint16_t>(_xfer.seq + 1u);
        if (_xfer.offset >= _xfer.size) {
            succeed();
            if (MultiByteClient* c = _client) {
                c->onMbTxDone();
            }
        }
        break;
    }
    }
}

inline void MultiByte::notifyResult() noexcept
{
    if (_client == nullptr) {
        return;
    }
    _client->onMbResult(_tag, _index, bufReceived(), _error);
}

} // namespace smcp::proto
