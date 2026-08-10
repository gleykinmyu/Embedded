/**
 * @file node.cpp
 */

#include "smcp/transport/node.hpp"

namespace smcp {

namespace detail {

void registerSession(Node& node, Session& session, uint8_t slot_id) noexcept
{
    const MISC::RegStatus st = node.sessions().registerAt(slot_id, &session);
    if (st != MISC::RegStatus::Ok) {
        node._status = Node::Status::RegisterFailed;
    }
}

} // namespace detail

Node::Node(BIF::CAN::ICAN& can, uint8_t node_id) noexcept
    : _link(can, node_id)
{}

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

    Outbound item{};
    item.body = body;
    item.dst_id = dst_id;
    item.pkt_id = pkt_id;

    if (_tx.push(item)) {
        if (_status == Status::TxQueueFull) {
            _status = Status::OK;
        }
        return;
    }

    /* Как nex::Application::enqueue: на лимите глубины — без вложенного update. */
    if (_updateDepth >= SMCP_MAX_UPDATE_DEPTH) {
        _status = Status::TxQueueFull;
        return;
    }

    MISC::MsTimer stall{};
    stall.start(clockMs(), SMCP_TX_STALL_MS);

    for (;;) {
        const std::size_t space_before = _tx.space();
        update();

        if (_tx.push(item)) {
            if (_status == Status::TxQueueFull) {
                _status = Status::OK;
            }
            return;
        }

        if (_tx.space() > space_before) {
            stall.start(clockMs(), SMCP_TX_STALL_MS);
        }

        if (stall.timedOut(clockMs())) {
            break;
        }
    }

    _status = Status::TxQueueFull;
}

bool Node::receive(msg::Packet& out) noexcept
{
    return _link.receive(out);
}

bool Node::acceptRx(const msg::Packet& pkt) noexcept
{
    /* Слушаем эфир с первого RX — до и после открытия сессий. */
    if (pkt.hdr.src_id == id()) {
        _status = Status::IdConflict;
        auto& reg = sessions();
        const uint8_t end = reg.endId();
        for (uint8_t sid = reg.firstId(); sid < end; ++sid) {
            Session* s = reg.get(sid);
            if (s != nullptr) {
                s->stop();
            }
        }
        return false;
    }

    return pkt.hdr.isAddressedTo(id());
}

void Node::update() noexcept
{
    if (_clock != nullptr) {
        _now_ms = _clock();
    }

    if (_updateDepth < SMCP_MAX_UPDATE_DEPTH) {
        ++_updateDepth;

        msg::Packet pkt;
        while (receive(pkt)) {
            if (!acceptRx(pkt)) {
                continue;
            }

            Session* s = sessionByPeer(pkt.hdr.src_id);
            if (s != nullptr && s->onPacket(pkt)) {
                continue;
            }

            onPacket(pkt);
        }

        auto& reg = sessions();
        const uint8_t end = reg.endId();
        for (uint8_t id = reg.firstId(); id < end; ++id) {
            Session* s = reg.get(id);
            if (s != nullptr) {
                s->tick();
            }
        }

        --_updateDepth;
    }

    while (const Outbound* item = _tx.peek()) {
        // TODO: приоритет / fairness между сессиями; таймаут головы при длительном LinkError
        if (!_link.send(item->body, item->dst_id, item->pkt_id)) {
            _status = Status::LinkError;
            return;
        }
        _tx.drop();
    }

    if (_status == Status::LinkError && _link.getStatus() == Link::Status::OK) {
        _status = Status::OK;
    }
}

} // namespace smcp
