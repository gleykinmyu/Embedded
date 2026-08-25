/**
 * @file node.cpp
 */

#include "smcp/transport/node.hpp"
#include "smcp/transport/session.hpp"

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
    const std::size_t cap = reg.capacity();
    const std::size_t ring = cap + 1u; /* сессии + bus */

    for (std::size_t tried = 0; tried < ring; ++tried) {
        const std::size_t slot = _drainCursor % ring;
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
        s->dropTx();
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
            if (s != nullptr) {
                if (s->onPacket(pkt)) {
                    continue;
                }
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

} // namespace smcp
