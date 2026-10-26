/**
 * @file session.cpp
 */

#include "smcp/session.hpp"
#include "smcp/node.hpp"
#include "smcp/debug.hpp"

namespace smcp {

Session::Session(Node& n) noexcept
    : node(n)
    , _reply(*this)
    , _link(*this)
    , _mb(*this)
    , _tagged(*this, _mb)
{
    detail::registerSession(n, *this);
}

void Session::open(uint8_t peer_id) noexcept
{
    if (!node.isReady() || peer_id == 0u) {
        SMCP_LOG(SMCP_SESS,
            "S%u rej p=%u rdy=%u\n",
            "[SMCP] Session[%u] open reject peer=%u ready=%u\n",
            static_cast<unsigned>(_id),
            static_cast<unsigned>(peer_id),
            node.isReady() ? 1u : 0u);
        return;
    }
    _tagged.abort(false);
    _reply.abort(false);
    clearTx();
    _txn_tx = 0;
    _peer_id = peer_id;
    _link.connect();
}

void Session::start(uint8_t peer_id) noexcept
{
    _master = true;
    open(peer_id);
}

void Session::close(Fault reason) noexcept
{
    if (_link.isIdle()) {
        return;
    }
    if (reason != Fault::None) {
        onFault(reason);
    }
    _tagged.abort();
    _reply.abort(); /* окно ждало Ack → onNack(Body, Abort) */
    _master = false;
    _txn_tx = 0;
    clearTx();
    _link.idle();
}

void Session::sendAck(uint8_t txn_id) noexcept
{
    if (!isOpen()) {
        return;
    }
    send(msg::Ack{}, txn_id);
}

void Session::sendNack(uint8_t txn_id, uint8_t error, uint32_t detail) noexcept
{
    if (!isOpen()) {
        return;
    }
    send(msg::Nack::make(error, detail), txn_id);
}

bool Session::onPacket(const Packet& pkt) noexcept
{
    const uint32_t now = node.clockMs();
    if (_link.onPacket(pkt, now)) {
        return true;
    }
    if (_reply.onPacket(pkt)) {
        return true;
    }
    if (_tagged.onPacket(pkt, now)) {
        return true;
    }
    /* DataBegin/Data вооружённого multi. Короткий Data своего Get забрал TaggedOps. */
    return _mb.onPacket(pkt, now);
}

void Session::onFault(Fault reason) noexcept
{
    node.onFault(this, reason);
}

void Session::onGet(const msg::Get& body, uint8_t txn_id) noexcept
{
    (void)body;
    sendNack(txn_id, msg::Nack::Error::NotSupported);
}

bool Session::onSetBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                          IBuf& buf) noexcept
{
    (void)tag;
    (void)txn_id;
    (void)size;
    (void)buf;
    return false;
}

void Session::onSet(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept
{
    (void)tag;
    (void)buf;
    (void)status;
}

void Session::onReply(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept
{
    (void)tag;
    (void)buf;
    (void)status;
}

void Session::tick() noexcept
{
    /* Тикаем и при LinkError/Listen: wall-clock таймеры; TX режет send/pump. */
    const uint32_t now = node.clockMs();
    _mb.tick(now); /* RX wd + TX peek→send */
    _tagged.tick(now);
    _reply.tick(now);
    _link.tick(now);
}

const TxSlot* Session::peekTx() noexcept
{
    const uint32_t now = node.clockMs();
    if (_rr_ctrl) {
        if (const TxSlot* p = peekCtrl()) {
            return p;
        }
        if (const TxSlot* p = _reply.peek(now)) {
            _out = OutSrc::Req;
            return p;
        }
    } else {
        if (const TxSlot* p = _reply.peek(now)) {
            _out = OutSrc::Req;
            return p;
        }
        if (const TxSlot* p = peekCtrl()) {
            return p;
        }
    }
    _out = OutSrc::None;
    return nullptr;
}

void Session::onTxResult(bool ok) noexcept
{
    if (!ok) {
        return;
    }
    switch (_out) {
    case OutSrc::Req:
        _reply.startAck(node.clockMs());
        break;
    case OutSrc::Ctrl:
        if (const TxSlot* p = _tx_ctrl.peek()) {
            if (!_mb.onTxResult(*p)) {
                _tagged.onTxResult(*p);
            }
        }
        _tx_ctrl.drop();
        break;
    case OutSrc::None:
        break;
    }
    if (_out != OutSrc::None) {
        _rr_ctrl = (_out != OutSrc::Ctrl);
    }
}

bool Session::enqueueReq(Message msg, uint8_t txn_id) noexcept
{
    if (!node.isReady() || _link.isIdle() || _peer_id == 0u || txn_id == 0u) {
        return false;
    }
    if (msg.dlc > Message::kMaxDlc) {
        msg.dlc = Message::kMaxDlc;
    }

    TxSlot item{};
    item.msg = msg;
    item.dst_id = _peer_id;
    item.txn_id = static_cast<uint8_t>(txn_id & protocol::kTxnIdMax);
    return _reply.enqueue(item);
}

bool Session::send(Message msg, uint8_t txn_id) noexcept
{
    if (!node.isReady() || _link.isIdle() || _peer_id == 0u) {
        return false;
    }
    if (msg.dlc > Message::kMaxDlc) {
        msg.dlc = Message::kMaxDlc;
    }

    TxSlot item{};
    item.msg = msg;
    item.dst_id = _peer_id;
    item.txn_id = static_cast<uint8_t>(txn_id & protocol::kTxnIdMax);

    const bool ok = _tx_ctrl.enqueue(item, node);
    if (!ok && _tx_ctrl.isFull()) {
        node.onTxFull(this);
    }
    return ok;
}

uint8_t Session::allocTxn() noexcept
{
    const uint8_t next = static_cast<uint8_t>(_txn_tx + 1u);
    _txn_tx = (next > protocol::kTxnIdMax) ? uint8_t{1} : next;
    return _txn_tx;
}

void Session::clearTx() noexcept
{
    _tx_ctrl.clear();
    _out = OutSrc::None;
    _rr_ctrl = true;
}

const TxSlot* Session::peekCtrl() noexcept
{
    if (const TxSlot* ctrl = _tx_ctrl.peek()) {
        _out = OutSrc::Ctrl;
        return ctrl;
    }
    return nullptr;
}

} // namespace smcp
