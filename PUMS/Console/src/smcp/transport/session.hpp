/**
 * @file session.hpp
 * @brief Сессия к одному peer: Heartbeat, session pkt_id, своя TX-очередь (PROTOCOL.md).
 *
 * Unicast TX — в Session::_tx (`isTxFull`); Node drain'ит started-сессии. Broadcast — Node::send.
 * IdConflict / dst — у Node::acceptRx.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "ms_timer.hpp"
#include "obj_registry.hpp"
#include "smcp/transport/node.hpp"

namespace smcp {

#ifndef SMCP_SESSION_TX_CAPACITY
#define SMCP_SESSION_TX_CAPACITY 16u
#endif

class Session {
public:
    /**
     * Регистрируется в Node::sessions() по @a slot_id (вызывать после storage сессий).
     */
    explicit Session(Node& node, uint8_t slot_id = 0) noexcept;

    /** Слот в Node::sessions() (ObjRegistry). */
    [[nodiscard]] uint8_t id() const noexcept { return _id; }

    void setPeerId(uint8_t peer_id) noexcept;

    void start() noexcept;
    void stop() noexcept;

    [[nodiscard]] bool isStarted() const noexcept { return _started; }
    [[nodiscard]] uint8_t peerId() const noexcept { return _peer_id; }
    [[nodiscard]] bool isOpen() const noexcept { return _open; }
    /** Session::_tx Full после stall; не поднимается в Node::TxQueueFull. */
    [[nodiscard]] bool isTxFull() const noexcept { return _tx.isFull(); }

    /** Unicast к peer. При requiresAck — pkt_id = ++TX. Без bool. */
    void send(const msg::Message& body) noexcept;
    void sendAck(uint8_t req_pkt_id) noexcept;
    void sendNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept;

    void tick() noexcept;

    /**
     * RX PDU сессии (HB / Ack/Nack). Кадр уже прошёл Node::acceptRx.
     * @return true — вызывающему не продолжать demux.
     */
    [[nodiscard]] bool onPacket(const msg::Packet& pkt) noexcept;

private:
    template <typename, typename>
    friend class MISC::ObjRegistry;
    friend class Node;

    void set_id(uint8_t id) noexcept { _id = id; }

    void sendPing() noexcept;
    /** @return true — в Session::_tx; при Full — stall Node::update. */
    [[nodiscard]] bool transmit(const msg::Message& body, uint8_t pkt_id = 0) noexcept;
    void onHeartbeat(const msg::Header& hdr) noexcept;
    void markDown() noexcept;

    [[nodiscard]] bool nodeOk() const noexcept;

    [[nodiscard]] const TxSlot* peekTx() const noexcept { return _tx.peek(); }
    void dropTx() noexcept { _tx.drop(); }
    /** Wire-результат головы (задел под pending/Ack). */
    void onTxResult(bool ok) noexcept;

    Node& _node;
    uint8_t _id = 0;
    uint8_t _peer_id = 0;
    uint8_t _pkt_tx = 0;
    bool _started = false;
    bool _open = false;
    bool _awaiting = false;

    MISC::MsTimer _timer{};
    TxQueue<SMCP_SESSION_TX_CAPACITY> _tx;
};

/** Банк Session[N]: ctor регистрирует каждый в Node::sessions() (слот = индекс). */
template <std::size_t N>
class SessionBank {
public:
    explicit SessionBank(Node& node) noexcept
        : SessionBank(node, std::make_index_sequence<N>{})
    {}

    [[nodiscard]] Session& operator[](std::size_t i) noexcept { return _items[i]; }
    [[nodiscard]] const Session& operator[](std::size_t i) const noexcept { return _items[i]; }

    [[nodiscard]] Session* begin() noexcept { return _items; }
    [[nodiscard]] Session* end() noexcept { return _items + N; }
    [[nodiscard]] const Session* begin() const noexcept { return _items; }
    [[nodiscard]] const Session* end() const noexcept { return _items + N; }

private:
    template <std::size_t... I>
    SessionBank(Node& node, std::index_sequence<I...>) noexcept
        : _items{Session{node, static_cast<uint8_t>(I)}...}
    {}

    Session _items[N];
};

} // namespace smcp
