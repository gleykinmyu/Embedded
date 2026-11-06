/**
 * @file node.cpp
 */

#include "smcp/node.hpp"
#include "smcp/session.hpp"
#include "smcp/debug.hpp"

#include <cstring>

namespace smcp {

// --- ctor ---

Node::Node(ILink& link, ClockFn clock) noexcept
    : _link(link)
    , _clock(clock)
{}

// --- статус / lifecycle ---

void Node::clearError() noexcept
{
    _link.clearError();
    setError(Error::None);
}

void Node::setName(const char* node_name) noexcept
{
    _name[0] = '\0';
    if (node_name == nullptr || node_name[0] == '\0') {
        return;
    }
    uint8_t i = 0;
    while (i < kNameMax && node_name[i] != '\0') {
        _name[i] = node_name[i];
        ++i;
    }
    _name[i] = '\0';
}

void Node::begin(uint8_t node_id) noexcept
{
    if (node_id == 0u || node_id == Packet::kBroadcastId) {
        return;
    }
    _link.setNodeId(node_id);
    _listen.stop();
    closeAllSessions();
    clearError();
    _listen.start(clockMs(), session::Heartbeat::kTimeoutMs);
    setState(State::Listen);
}

void Node::end() noexcept
{
    _listen.stop();
    closeAllSessions();
    clearError();
    setState(State::Idle);
}

// --- сессии (lookup) ---

Session* Node::sessionByPeer(uint8_t peer_id) noexcept
{
    if (peer_id == 0u) {
        return nullptr;
    }

    Session* found = nullptr;
    sessions().forEach([&](Session& s, uint8_t) noexcept -> bool {
        if (s.peerId() == peer_id && s.getStatus() != Session::Status::Idle) {
            found = &s;
            return true; /* стоп */
        }
        return false;
    });
    return found;
}

Session* Node::idleSession() noexcept
{
    Session* found = nullptr;
    sessions().forEach([&](Session& s, uint8_t) noexcept -> bool {
        if (s.getStatus() == Session::Status::Idle) {
            found = &s;
            return true; /* стоп */
        }
        return false;
    });
    return found;
}

// --- TX / pump (app) ---

void Node::send(Message msg, uint8_t dst_id, uint8_t txn_id) noexcept
{
    if (_state == State::Idle) {
        return;
    }

    if (msg.dlc > Message::kMaxDlc) {
        msg.dlc = Message::kMaxDlc;
    }

    TxSlot item{};
    item.msg = msg;
    item.dst_id = dst_id;
    item.txn_id = txn_id;

    if (!_tx.enqueue(item, *this) && _tx.isFull()) {
        onTxFull(nullptr);
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

        if (_updateDepth == 1u && _state == State::Listen && _listen.timedOut(_now_ms)) {
            _listen.stop();
            if (_state == State::Listen) {
                setState(State::Ready);
            }
        }

        --_updateDepth;
    } else {
        /* Вложенный update на лимите глубины — только TX, без tick/RX. */
        pumpTx(/*do_tick=*/false);
    }
}

void Node::closeAllSessions(Link link) noexcept
{
    sessions().forEach([&](Session& s, uint8_t) noexcept {
        s.close(link);
    });
}

// --- hooks (leaf / app) ---

void Node::onPacket(const Packet& pkt) noexcept
{
    SMCP_IF_MSG(session::Fault) {
        /* IdConflict: src = спорный id, сессию не рвём. Иной Fault — peer ушёл. */
        Session* const s = sessionByPeer(pkt.src_id);
        char name[session::Fault::kDetailMax + 1u]{};
        std::memcpy(name, body.detail, body.detail_len);
        SMCP_LOG(SMCP_NODE,
            "Ft %s p=%u n=%s\n",
            "[SMCP] Node::onPacket Fault %s peer=%u name=\"%s\"\n",
            session::Fault::cstr(body.error),
            static_cast<unsigned>(pkt.src_id),
            name);
        if (body.error != session::Fault::kIdConflict && s != nullptr) {
            s->close(Link::PeerDown);
        }
        return;
    }
}

void Node::onState(State state) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Nd %s id=%u\n",
        "[SMCP] Node::onState %s (id=%u)\n",
        cstr(state), static_cast<unsigned>(id()));
}

void Node::onError(Error error) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "NdErr %s id=%u\n",
        "[SMCP] Node::onError %s (id=%u)\n",
        cstr(error), static_cast<unsigned>(id()));
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
        "TxFull p=%u\n",
        "[SMCP] Node::onTxFull peer=%u\n",
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

void Node::onLink(Session* session, Link link) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Lk p=%u %s\n",
        "[SMCP] Node::onLink peer=%u %s\n",
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        cstr(link));
}

void Node::onAck(Session* session, const TxSlot& req, const session::Ack& reply) noexcept
{
    (void)reply;
    SMCP_LOG(SMCP_NODE,
        "Ack p=%u #%u %s\n",
        "[SMCP] Node::onAck peer=%u pkt=%u req=%s\n",
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        static_cast<unsigned>(req.txn_id),
        session::cstr(req.msg.id));
}

void Node::onNack(Session* session, const TxSlot& req, const session::Nack& reply) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Nk p=%u #%u %s %u %08lX\n",
        "[SMCP] Node::onNack peer=%u pkt=%u req=%s error=%u detail=0x%08lX\n",
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        static_cast<unsigned>(req.txn_id),
        session::cstr(req.msg.id),
        static_cast<unsigned>(static_cast<uint8_t>(reply.error)),
        static_cast<unsigned long>(reply.detail));
}

void Node::onPackFailed(Session* session, uint8_t msg_id) noexcept
{
    SMCP_LOG(SMCP_NODE,
        "Pack? p=%u %s(%u)\n",
        "[SMCP] Node::onPackFailed peer=%u id=%s (%u)\n",
        session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
        session::cstr(msg_id),
        static_cast<unsigned>(msg_id));
}

void Node::setState(State state) noexcept
{
    if (state == _state) {
        return;
    }
    _state = state;
    if (_state == State::Idle) {
        _listen.stop();
    }
    onState(_state);
}

void Node::setError(Error error) noexcept
{
    if (error == _error) {
        return;
    }
    _error = error;
    onError(_error);
}

// --- RX фильтр / IdConflict ---

bool Node::acceptRx(const Packet& pkt) noexcept
{
    /* Чужой кадр с моим src → IdConflict. Исключение: Fault IdConflict —
     * уход новичка с тем же id; иначе Ready-инкумбент суицидится. */
    if (pkt.src_id == id() && id() != 0u && _state != State::Idle) {
        if (pkt.msg.id == session::Fault::kId) {
            session::Fault body{};
            if (body.unpack(pkt.msg) && body.error == session::Fault::kIdConflict) {
                return false;
            }
        }
        enterIdConflict();
        return false;
    }

    if (_state == State::Idle) {
        return false;
    }

    return pkt.isAddressedTo(id());
}

void Node::enterIdConflict() noexcept
{
    if (_error == Error::IdConflict) {
        return;
    }

    SMCP_LOG(SMCP_NODE,
        "Ft IdConflict id=%u\n",
        "[SMCP] Node::enterIdConflict (id=%u)\n",
        static_cast<unsigned>(id()));
    session::Fault body{};
    body.error = session::Fault::kIdConflict;
    body.setDetailStr(_name);
    send(body); /* ещё Listen/Ready — send в Idle уже нет */
    closeAllSessions();
    pumpTx(/*do_tick=*/false);
    _tx.clear();
    _listen.stop();
    setError(Error::IdConflict);
    setState(State::Idle);
}

// --- TX внутренности ---

bool Node::sendWire(const TxSlot& item) noexcept
{
    if (_state == State::Idle) {
        return false;
    }

    Packet pkt{};
    pkt.dst_id = item.dst_id;
    pkt.txn_id = item.txn_id;
    pkt.msg = item.msg;

    /* Fail — только ILink::getStatus(); State не трогаем (можно снова пробовать). */
    return _link.send(pkt);
}

void Node::pumpTx(bool do_tick) noexcept
{
    SessionSlots& slots = sessions();
    const uint8_t cap = slots.capacity();
    const uint8_t ring = static_cast<uint8_t>(cap + 1u); /* сессии + bus */

    for (uint8_t tried = 0; tried < ring; ++tried) {
        const uint8_t slot = static_cast<uint8_t>(_drainCursor % ring);
        _drainCursor = static_cast<uint8_t>((_drainCursor + 1u) % ring);

        if (slot == cap) {
            if (_state == State::Idle) {
                continue;
            }
            const TxSlot* item = _tx.peek();
            if (item == nullptr) {
                continue;
            }
            if (!sendWire(*item)) {
                onTxResult(false);
                continue;
            }
            _tx.drop();
            onTxResult(true);
            continue;
        }

        Session* s = slots.at(slot);
        if (s == nullptr) {
            continue;
        }

        if (do_tick) {
            s->tick();
        }

        if (_state != State::Ready || s->getStatus() == Session::Status::Idle) {
            continue;
        }

        const TxSlot* item = s->peekTx();
        if (item == nullptr) {
            continue;
        }

        if (!sendWire(*item)) {
            s->onTxResult(false);
            continue;
        }
        /* Drop / pending Ack — решает Session::onTxResult. */
        s->onTxResult(true);
    }
}

Session* Node::openNewSession(const Packet& pkt) noexcept
{
    if (_state != State::Ready) {
        return nullptr;
    }
    if (pkt.src_id == 0u) {
        return nullptr;
    }
    if (pkt.msg.id != session::Heartbeat::kId) {
        return nullptr;
    }
    if (pkt.dst_id != id()) {
        return nullptr; /* bind только unicast, не broadcast */
    }

    Session* const slot = idleSession();
    if (slot == nullptr) {
        onSessionFull(pkt.src_id);
    }
    return slot;
}

// --- TxQueue ---

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
