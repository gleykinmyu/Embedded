/**
 * @file session.hpp
 * @brief Сессия к одному peer: Heartbeat, session pkt_id, своя TX-очередь (PROTOCOL.md).
 *
 * Unicast TX — в Session::_tx (`isTxFull`); Node drain'ит, пока не Idle. Broadcast — Node::send.
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
     * Линк, не шкала жёсткости (в отличие от Node::Status).
     * start: Idle→Connecting (первый исходящий ping). RX HB → Open.
     * closeLink: Open/Connecting→Connecting (reconnect). stop → Idle.
     */
    enum class Status : uint8_t {
        Idle = 0,    /**< нет сессии / stop */
        Connecting,  /**< ждём первый HB / reconnect */
        Open,        /**< обмен HB */
    };

    /**
     * Регистрируется в Node::sessions() в первый свободный слот
     * (вызывать после storage сессий).
     */
    explicit Session(Node& node) noexcept;

    /** Слот в Node::sessions() (ObjRegistry), не peer на шине. */
    [[nodiscard]] uint8_t id() const noexcept { return _id; }

    /** Сменить peer; closeLink (очередь/таймер, Connecting если не Idle). */
    void setPeerId(uint8_t peer_id) noexcept;

    /** Idle→Connecting. Первый ping — из tick, когда есть peer. */
    void start() noexcept;
    /** Idle + closeLink (без reconnect). */
    void stop() noexcept;

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    [[nodiscard]] uint8_t peerId() const noexcept { return _peer_id; }
    /** Sticky Full после stall; каждая неудачная постановка → Node::onTxFull(this). */
    [[nodiscard]] bool isTxFull() const noexcept { return _tx.isFull(); }

    /** Unicast к peer. requiresAck: pkt_id = _pkt_tx, ++ только если кадр в очереди. */
    void send(const msg::Message& body) noexcept;
    void sendAck(uint8_t req_pkt_id) noexcept;
    void sendNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept;

    /** Keep-alive: timeout → closeLink + ping; иначе первый ping после start. */
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
    /** @return true — в Session::_tx; при Full — stall Node::update, затем onTxFull. */
    [[nodiscard]] bool transmit(const msg::Message& body, uint8_t pkt_id = 0) noexcept;
    void onHeartbeat(const msg::Header& hdr) noexcept;
    /** Сброс очереди/таймера/_awaiting. Не Idle → Connecting (reconnect). */
    void closeLink() noexcept;
    void setStatus(Status status) noexcept;

    [[nodiscard]] bool nodeOk() const noexcept;

    [[nodiscard]] const TxSlot* peekTx() const noexcept { return _tx.peek(); }
    void dropTx() noexcept { _tx.drop(); }
    /** Wire-результат головы (задел под pending/Ack). */
    void onTxResult(bool ok) noexcept;

    Node& _node;
    uint8_t _id = 0;       /**< слот registry */
    uint8_t _peer_id = 0;  /**< SRC удалённого узла (шина) */
    uint8_t _pkt_tx = 0;   /**< следующий pkt_id для requiresAck */
    Status _status = Status::Idle;
    bool _awaiting = false; /**< ждём pong на наш ping (не отвечать эхом) */

    MISC::MsTimer _timer{}; /**< ping: дедлайн pong; RX HB: тишина до timeout */
    TxQueue<SMCP_SESSION_TX_CAPACITY> _tx;
};

/** Банк Session[N]: ctor регистрирует каждый в Node::sessions() подряд. */
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
        : _items{((void)I, Session{node})...}
    {}

    Session _items[N];
};

} // namespace smcp
