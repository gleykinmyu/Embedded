/**
 * @file reply_wnd.cpp
 */

#include "smcp/session/reply_wnd.hpp"

#include "smcp/session.hpp"

namespace smcp {

ReplyWnd::ReplyWnd(Session& session) noexcept
    : _s(session)
{}

bool ReplyWnd::enqueue(const TxSlot& item) noexcept
{
    const bool ok = _q.enqueue(item, _s.node);
    if (!ok && _q.isFull()) {
        _s.node.onTxFull(&_s);
    }
    return ok;
}

void ReplyWnd::abort(bool notify) noexcept
{
    if (notify && _ack.isWaiting()) {
        if (const TxSlot* h = _q.peek()) {
            _s.nack(*h, msg::Nack::Error::Body,
                    static_cast<uint32_t>(MultiByte::Error::Abort));
        }
    }
    _q.clear();
    _ack.clear();
}

const TxSlot* ReplyWnd::peek(uint32_t now_ms) noexcept
{
    if (_ack.isWaiting()) {
        if (!_ack.timedOut(now_ms) || _ack.isReplyLimit()) {
            return nullptr;
        }
    }
    return _q.peek();
}

void ReplyWnd::startAck(uint32_t now_ms) noexcept
{
    _ack.start(now_ms, msg::Ack::kTimeoutMs);
}

bool ReplyWnd::onPacket(const Packet& pkt) noexcept
{
    if (pkt.msg.id != msg::Ack::kId && pkt.msg.id != msg::Nack::kId) {
        return false;
    }
    if (!_ack.isWaiting()) {
        return false;
    }
    const TxSlot* h = _q.peek();
    if (h == nullptr || h->txn_id != pkt.txn_id) {
        return false;
    }

    SMCP_IF_MSG(msg::Nack) {
        _s.nack(*h, body);
        _ack.clear();
        _q.drop();
        return true;
    } else SMCP_IF_MSG(msg::Ack) {
        _s.ack(*h, body);
        _ack.clear();
        _q.drop();
        return true;
    }
    /* unpack fail — окно не трогаем. */
    return false;
}

void ReplyWnd::tick(uint32_t now_ms) noexcept
{
    if (!_ack.timedOut(now_ms) || !_ack.isReplyLimit()) {
        return;
    }
    const TxSlot* h = _q.peek();
    _ack.clear();
    if (h == nullptr) {
        return;
    }
    _s.nack(*h, msg::Nack::kTimeout);
    _q.drop();
}

} // namespace smcp
