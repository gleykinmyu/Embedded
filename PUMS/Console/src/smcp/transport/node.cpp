/**
 * @file node.cpp
 */

#include "smcp/transport/node.hpp"
#include "smcp/transport/session.hpp"
#include "smcp/debug.hpp"

#include <variant>

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
    setStatus(Status::OK);
}

void Node::setStatus(Status status) noexcept
{
    if (status == _status) {
        return;
    }
    /* clearError → OK всегда; иначе не понижаем (порядок Status = жёсткость). */
    if (status != Status::OK && status <= _status) {
        return;
    }

    _status = status;
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

Session* Node::openNewSession(const msg::Packet& pkt) noexcept
{
    if (std::get_if<msg::Heartbeat>(&pkt.body) == nullptr) {
        return nullptr;
    }
    if (pkt.hdr.dst_id != id()) {
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
    onSessionFull(pkt.hdr.src_id);
    return nullptr;
}

void Node::send(const msg::Message& body, uint8_t dst_id, uint8_t pkt_id) noexcept
{
    if (_status == Status::IdConflict) {
        return;
    }

    TxSlot item{};
    item.body = body;
    item.dst_id = dst_id;
    item.pkt_id = pkt_id;

    if (!_tx.enqueue(item, *this) && _tx.isFull()) {
        onTxFull(nullptr);
    }
}

bool Node::acceptRx(const msg::Packet& pkt) noexcept
{
    /* Слушаем эфир с первого RX — до и после открытия сессий. */
    if (pkt.hdr.src_id == id()) {
        setStatus(Status::IdConflict);
        return false;
    }

    if (_status == Status::IdConflict) {
        return false;
    }

    return pkt.hdr.isAddressedTo(id());
}

bool Node::sendWire(const TxSlot& item) noexcept
{
    if (_status == Status::IdConflict) {
        return false;
    }

    if (!_link.send(item.body, item.dst_id, item.pkt_id)) {
        setStatus(Status::LinkError);
        return false;
    }
    if (_status == Status::LinkError) {
        setStatus(Status::OK);
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

        if (_status == Status::IdConflict || s->getStatus() == Session::Status::Idle) {
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

        msg::Packet pkt;
        while (_link.receive(pkt)) {
            if (!acceptRx(pkt)) {
                continue;
            }

            Session* s = sessionByPeer(pkt.hdr.src_id);
            if (s == nullptr) {
                s = openNewSession(pkt);
            }
            if (s != nullptr && s->onPacket(pkt)) {
                continue;
            }

            onPacket(pkt);
        }

        pumpTx(/*do_tick=*/true);

        --_updateDepth;
    } else {
        /* Вложенный update на лимите глубины — только TX, без tick/RX. */
        pumpTx(/*do_tick=*/false);
    }
}

void Node::onAck(Session* session, const TxSlot& req) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    SMCP_NODE("Ack s=%u p=%u #%u %s\n",
             session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
             session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
             static_cast<unsigned>(req.pkt_id),
             msg::cstrS(msg::helpers::msgIdOf(req.body)));
#else
    SMCP_NODE("[SMCP] Node::onAck session=%u peer=%u pkt=%u req=%s\n",
             session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
             session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
             static_cast<unsigned>(req.pkt_id),
             msg::cstr(msg::helpers::msgIdOf(req.body)));
#endif
}

void Node::onStatus(Status status) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    SMCP_NODE("Nd %s id=%u\n",
#else
    SMCP_NODE("[SMCP] Node::onStatus %s (id=%u)\n",
#endif
             cstr(status), static_cast<unsigned>(id()));
}

void Node::onTxFull(Session* session) noexcept
{
    if (session == nullptr) {
#if defined(SMCP_TRACE_SHORT)
        SMCP_NODE("TxFull bus id=%u\n", static_cast<unsigned>(id()));
#else
        SMCP_NODE("[SMCP] Node::onTxFull bus (id=%u)\n", static_cast<unsigned>(id()));
#endif
        return;
    }
#if defined(SMCP_TRACE_SHORT)
    SMCP_NODE("TxFull s=%u p=%u\n",
#else
    SMCP_NODE("[SMCP] Node::onTxFull session=%u peer=%u\n",
#endif
             static_cast<unsigned>(session->id()),
             static_cast<unsigned>(session->peerId()));
}

void Node::onSessionFull(uint8_t peer_id) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    SMCP_NODE("SessFull p=%u id=%u\n",
#else
    SMCP_NODE("[SMCP] Node::onSessionFull peer=%u (id=%u)\n",
#endif
             static_cast<unsigned>(peer_id),
             static_cast<unsigned>(id()));
}

void Node::onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    SMCP_NODE("Nk s=%u p=%u #%u %s %s %u\n",
             session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
             session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
             static_cast<unsigned>(req.pkt_id),
             msg::cstrS(msg::helpers::msgIdOf(req.body)),
             msg::cstrS(reply.code),
             static_cast<unsigned>(reply.detail));
#else
    SMCP_NODE("[SMCP] Node::onNack session=%u peer=%u pkt=%u req=%s code=%s detail=%u\n",
             session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
             session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
             static_cast<unsigned>(req.pkt_id),
             msg::cstr(msg::helpers::msgIdOf(req.body)),
             msg::cstr(reply.code),
             static_cast<unsigned>(reply.detail));
#endif
}

void Node::onPktIdMismatch(Session* session, uint8_t expected, uint8_t got) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    SMCP_NODE("PktId s=%u p=%u %u!=%u\n",
#else
    SMCP_NODE("[SMCP] Node::onPktIdMismatch session=%u peer=%u expect=%u got=%u\n",
#endif
             session != nullptr ? static_cast<unsigned>(session->id()) : 0u,
             session != nullptr ? static_cast<unsigned>(session->peerId()) : 0u,
             static_cast<unsigned>(expected),
             static_cast<unsigned>(got));
}

} // namespace smcp
