/**
 * @file node.cpp
 */

#include "smcp/node.hpp"
#include "smcp/session.hpp"
#include "smcp/debug.hpp"

namespace smcp {

namespace detail {

void registerSession(Node& node, Session& session) noexcept
{
    uint8_t id = 0;
    const MISC::RegStatus st = node.sessions().registerAuto(&session, id);
    if (st != MISC::RegStatus::Ok) {
        node.setStatus(Node::Status::RegisterFailed);
    }
}

} // namespace detail

Node::Node(ILink& link, ClockFn clock) noexcept
    : _link(link)
    , _clock(clock)
{}

void Node::clearError() noexcept
{
    _link.clearError();
    setStatus(Status::Idle);
}

void Node::begin(uint8_t node_id) noexcept
{
    if (node_id == 0u || node_id == Packet::kBroadcastId) {
        return;
    }
    _link.setNodeId(node_id);
    _listen.stop();
    closeAllSessions(Fault::None);
    if (_status >= Status::LinkError) {
        clearError();
    }
    _listen.start(clockMs(), msg::Heartbeat::kTimeoutMs);
    setStatus(Status::Listen);
}

void Node::end() noexcept
{
    _listen.stop();
    closeAllSessions(Fault::None);
    if (_status >= Status::LinkError) {
        clearError();
    }
    setStatus(Status::Idle);
}

void Node::setStatus(Status status) noexcept
{
    if (status == _status) {
        return;
    }
    /* Отказ не затираем младшим статусом. Снять — Idle. LinkError → Ready после удачной TX. */
    if (isFault() && status < _status && status != Status::Idle
        && !(_status == Status::LinkError && status == Status::Ready)) {
        return;
    }

    _status = status;
    if (_status >= Status::LinkError) {
        _listen.stop();
    }
    onStatus(_status);
}

Session* Node::sessionByPeer(uint8_t peer_id) noexcept
{
    if (peer_id == 0u) {
        return nullptr;
    }

    auto& reg = sessions();
    const uint8_t end = reg.endId();
    for (uint8_t id = reg.firstId(); id < end; ++id) {
        Session* s = reg.get(id);
        if (s != nullptr && s->peerId() == peer_id) {
            return s;
        }
    }
    return nullptr;
}

Session* Node::openNewSession(const Packet& pkt) noexcept
{
    if (_status != Status::Ready) {
        return nullptr;
    }
    if (pkt.msg.id != msg::Heartbeat::kId) {
        return nullptr;
    }
    if (pkt.dst_id != id()) {
        return nullptr; /* bind только unicast, не broadcast */
    }

    auto& reg = sessions();
    const uint8_t end = reg.endId();
    for (uint8_t sid = reg.firstId(); sid < end; ++sid) {
        Session* slot = reg.get(sid);
        if (slot != nullptr && slot->peerId() == 0u) {
            return slot;
        }
    }
    onSessionFull(pkt.src_id);
    return nullptr;
}

void Node::send(Message msg, uint8_t dst_id, uint8_t pkt_id) noexcept
{
    if (_status == Status::IdConflict || _status == Status::Idle) {
        return;
    }

    if (msg.dlc > Message::kMaxDlc) {
        msg.dlc = Message::kMaxDlc;
    }

    TxSlot item{};
    item.msg = msg;
    item.dst_id = dst_id;
    item.pkt_id = pkt_id;

    if (!_tx.enqueue(item, *this) && _tx.isFull()) {
        onTxFull(nullptr);
    }
}

bool Node::acceptRx(const Packet& pkt) noexcept
{
    /* Слушаем эфир с первого RX — до и после открытия сессий. */
    if (pkt.src_id == id() && id() != 0u && _status != Status::Idle) {
        enterIdConflict();
        return false;
    }

    if (_status == Status::Idle || _status == Status::IdConflict) {
        return false;
    }

    return pkt.isAddressedTo(id());
}

void Node::closeAllSessions(Fault reason) noexcept
{
    auto& reg = sessions();
    const uint8_t end = reg.endId();
    for (uint8_t sid = reg.firstId(); sid < end; ++sid) {
        Session* const s = reg.get(sid);
        if (s != nullptr) {
            s->close(reason);
        }
    }
}

void Node::enterIdConflict() noexcept
{
    if (_status == Status::IdConflict) {
        return;
    }

    SMCP_LOG(SMCP_NODE,
        "Ft IdConflict id=%u\n",
        "[SMCP] Node::enterIdConflict (id=%u)\n",
        static_cast<unsigned>(id()));
    msg::Fault body{};
    body.error = msg::Fault::kIdConflict;
    send(body);
    closeAllSessions(Fault::IdConflict);
    pumpTx(/*do_tick=*/false);
    _tx.clear();
    _listen.stop();
    onFault(nullptr, Fault::IdConflict);
    setStatus(Status::IdConflict);
}

bool Node::sendWire(const TxSlot& item) noexcept
{
    if (_status == Status::IdConflict || _status == Status::Idle) {
        return false;
    }

    Packet pkt{};
    pkt.dst_id = item.dst_id;
    pkt.pkt_id = item.pkt_id;
    pkt.msg = item.msg;

    if (!_link.send(pkt)) {
        setStatus(Status::LinkError);
        return false;
    }
    if (_status == Status::LinkError) {
        setStatus(Status::Ready);
    }
    return true;
}

void Node::pumpTx(bool do_tick) noexcept
{
    auto& reg = sessions();
    const uint8_t first = reg.firstId();
    const uint8_t cap = static_cast<uint8_t>(reg.capacity());
    const uint8_t ring = static_cast<uint8_t>(cap + 1u); /* сессии + bus */

    for (uint8_t tried = 0; tried < ring; ++tried) {
        const uint8_t slot = static_cast<uint8_t>(_drainCursor % ring);
        _drainCursor = static_cast<uint8_t>((_drainCursor + 1u) % ring);

        if (slot == cap) {
            if (_status == Status::IdConflict) {
                continue;
            }
            const TxSlot* item = _tx.peek();
            if (item == nullptr) {
                continue;
            }
            if (!sendWire(*item)) {
                onTxResult(false);
                return;
            }
            _tx.drop();
            onTxResult(true);
            continue;
        }

        Session* s = reg.get(static_cast<uint8_t>(first + slot));
        if (s == nullptr) {
            continue;
        }

        if (do_tick) {
            s->tick();
        }

        if (_status != Status::Ready || s->getStatus() == Session::Status::Idle) {
            continue;
        }

        const TxSlot* item = s->peekTx();
        if (item == nullptr) {
            continue;
        }

        if (!sendWire(*item)) {
            s->onTxResult(false);
            return;
        }
        /* Drop / pending Ack — решает Session::onTxResult. */
        s->onTxResult(true);
    }
}

void Node::update() noexcept
{
    _now_ms = clockMs();

    if (_updateDepth < SMCP_MAX_UPDATE_DEPTH) {
        ++_updateDepth;

        Packet pkt;
        while (_link.receive(pkt)) {
            if (!acceptRx(pkt)) {
                continue;
            }

            Session* s = sessionByPeer(pkt.src_id);
            if (s == nullptr) {
                s = openNewSession(pkt);
            }
            if (s != nullptr && s->onPacket(pkt)) {
                continue;
            }

            onPacket(pkt);
        }

        pumpTx(/*do_tick=*/true);

        if (_updateDepth == 1u && _status == Status::Listen && _listen.timedOut(_now_ms)) {
            _listen.stop();
            if (_status == Status::Listen) {
                setStatus(Status::Ready);
            }
        }

        --_updateDepth;
    } else {
        /* Вложенный update на лимите глубины — только TX, без tick/RX. */
        pumpTx(/*do_tick=*/false);
    }
}

void Node::onAck(Session* session, const TxSlot& req, const msg::Ack& reply) noexcept
{
    (void)reply;
    SMCP_LOG(SMCP_NODE,
        "Ack s=%u p=%u #%u %s\n",
        "[SMCP] Node::onAck session=%u peer=%u pkt=%u req=%s\n",
        session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        static_cast<unsigned>(req.pkt_id),
        SMCP_PICK(msg::cstrMsgS(req.msg.id), msg::cstrMsg(req.msg.id)));
}

void Node::onStatus(Status status) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Nd %s id=%u\n",
        "[SMCP] Node::onStatus %s (id=%u)\n",
        cstr(status), static_cast<unsigned>(id()));
}

void Node::onFault(Session* session, Fault reason) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Ft %s s=%u p=%u\n",
        "[SMCP] Node::onFault %s session=%u peer=%u\n",
        cstr(reason),
        session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u);
}

void Node::onLink(Session* session, bool up) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Lk s=%u p=%u %u\n",
        "[SMCP] Node::onLink session=%u peer=%u up=%u\n",
        session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        up ? 1u : 0u);
}

void Node::onPacket(const Packet& pkt) noexcept
{
    SMCP_IF_MSG(msg::Fault) {
        Session* const s = sessionByPeer(pkt.src_id);
        SMCP_LOG(SMCP_NODE,
            "Ft %s s=%u p=%u\n",
            "[SMCP] Node::onPacket Fault %s session=%u peer=%u\n",
            msg::Fault::cstr(body.error),
            s != nullptr ? static_cast<unsigned>(s->id()) : 0u,
            static_cast<unsigned>(pkt.src_id));
        if (body.error == msg::Fault::kIdConflict && s != nullptr) {
            s->close(Fault::IdConflict);
        }
    }
}

void Node::onTxFull(Session* session) noexcept
{
    if (session == nullptr) {
        SMCP_LOG(SMCP_NODE,
            "TxFull bus id=%u\n",
            "[SMCP] Node::onTxFull bus (id=%u)\n",
            static_cast<unsigned>(id()));
        return;
    }
    SMCP_LOG(SMCP_NODE,
        "TxFull s=%u p=%u\n",
        "[SMCP] Node::onTxFull session=%u peer=%u\n",
        static_cast<unsigned>(session->id()),
        static_cast<unsigned>(session->peerId()));
}

void Node::onSessionFull(uint8_t peer_id) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "SessFull p=%u id=%u\n",
        "[SMCP] Node::onSessionFull peer=%u (id=%u)\n",
        static_cast<unsigned>(peer_id),
        static_cast<unsigned>(id()));
}

void Node::onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Nk s=%u p=%u #%u %s %u %08lX\n",
        "[SMCP] Node::onNack session=%u peer=%u pkt=%u req=%s error=%u detail=0x%08lX\n",
        session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        static_cast<unsigned>(req.pkt_id),
        SMCP_PICK(msg::cstrMsgS(req.msg.id), msg::cstrMsg(req.msg.id)),
        static_cast<unsigned>(reply.error),
        static_cast<unsigned long>(reply.detail));
}

void Node::onPktIdMismatch(Session* session, uint8_t expected, uint8_t got) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "PktId s=%u p=%u %u!=%u\n",
        "[SMCP] Node::onPktIdMismatch session=%u peer=%u expect=%u got=%u\n",
        session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        static_cast<unsigned>(expected),
        static_cast<unsigned>(got));
}

void Node::onPackFailed(Session* session, uint8_t msg_id) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Pack? s=%u p=%u %s(%u)\n",
        "[SMCP] Node::onPackFailed session=%u peer=%u id=%s (%u)\n",
        session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        SMCP_PICK(msg::cstrMsgS(msg_id), msg::cstrMsg(msg_id)),
        static_cast<unsigned>(msg_id));
}

template <std::size_t Cap>
const TxSlot* TxQueue<Cap>::peek() const noexcept
{
    return _q.peek();
}

template <std::size_t Cap>
void TxQueue<Cap>::drop() noexcept
{
    _q.drop();
}

template <std::size_t Cap>
void TxQueue<Cap>::clear() noexcept
{
    _q.clear();
    _full = false;
}

template <std::size_t Cap>
bool TxQueue<Cap>::isFull() const noexcept
{
    return _full;
}

template <std::size_t Cap>
std::size_t TxQueue<Cap>::size() const noexcept
{
    return _q.size();
}

template <std::size_t Cap>
std::size_t TxQueue<Cap>::space() const noexcept
{
    return _q.space();
}

template <std::size_t Cap>
bool TxQueue<Cap>::empty() const noexcept
{
    return _q.empty();
}

template class TxQueue<SMCP_TX_QUEUE_CAPACITY>;
template class TxQueue<SMCP_SESSION_CTRL_TX_CAPACITY>;
#if SMCP_SESSION_TX_CAPACITY != SMCP_TX_QUEUE_CAPACITY \
    && SMCP_SESSION_TX_CAPACITY != SMCP_SESSION_CTRL_TX_CAPACITY
template class TxQueue<SMCP_SESSION_TX_CAPACITY>;
#endif

} // namespace smcp
