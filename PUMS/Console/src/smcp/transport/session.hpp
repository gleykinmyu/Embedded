/**
 * @file session.hpp
 * @brief Сессия к одному peer: Heartbeat, session pkt_id, TX (PROTOCOL.md).
 *
 * Unicast TX — внутри `_tx_req` / `_tx_ctrl`, снаружи send/isTxFull.
 * Pump (tick/peekTx/onTxResult) — только Node (`friend`).
 * onPacket — Node + наследник (сервер: SessionConsole).
 * Класс A: окно 1 — голова `_tx_req` + `_ack`.
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

#ifndef SMCP_SESSION_CTRL_TX_CAPACITY
#define SMCP_SESSION_CTRL_TX_CAPACITY 8u
#endif

class Session {
public:
    /**
     * Линк. _master (start) — keep-alive ping.
     * Idle / Connecting / Awaiting / Open; isOpen = только Open.
     * misses ≥ N → onHbLost + close.
     */
    enum class Status : uint8_t {
        Idle = 0,
        Connecting, /**< нет линка: master шлёт первый ping */
        Awaiting,   /**< ping ушёл, ждём первый pong (ещё не up) */
        Open,       /**< линк подтверждён RX HB */
    };

    // --- ctor / id ---

    virtual ~Session() = default;

    /** Регистрация в Node::sessions() (первый свободный слот). */
    explicit Session(Node& node) noexcept;

    /** Слот в Node::sessions(), не peer на шине. */
    [[nodiscard]] uint8_t id() const noexcept { return _id; }

    // --- состояние (app) ---

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    [[nodiscard]] uint8_t peerId() const noexcept { return _peer_id; }
    /**
     * Линк к peer (Open). При IdConflict Node режет TX/RX;
     * isOpen может остаться true — close в onStatus наверху.
     */
    [[nodiscard]] bool isOpen() const noexcept { return _status == Status::Open; }
    [[nodiscard]] bool isMaster() const noexcept { return _master; }
    [[nodiscard]] bool isTxFull() const noexcept
    {
        return _tx_req.isFull() || _tx_ctrl.isFull();
    }
    [[nodiscard]] bool isAckPending() const noexcept { return _ack.isWaiting(); }

    // --- жизненный цикл (app) ---

    /**
     * Сброс TX/таймеров + peer → Connecting.
     * Master: из start. Slave: из onHeartbeat при !isOpen.
     */
    void open(uint8_t peer_id) noexcept;
    /** `_master` + abortAck + open. Ping — из tick (Connecting). */
    void start(uint8_t peer_id) noexcept;
    /** Idle, peer=0, сброс TX/таймеров/ack; снимает _master. */
    void close() noexcept;

    // --- исходящие PDU (app) ---

    /**
     * Unicast к peer при isOpen().
     * requiresAck → `_tx_req` + pkt_id; иначе `_tx_ctrl`.
     */
    void send(const msg::Message& body) noexcept;
    void sendAck(uint8_t req_pkt_id) noexcept;
    void sendNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept;

protected:
    /** HB / Ack / Nack. true — Node не зовёт Node::onPacket. */
    [[nodiscard]] virtual bool onPacket(const msg::Packet& pkt) noexcept;

private:
    friend class Node;
    template <typename, typename>
    friend class MISC::ObjRegistry;

    void set_id(uint8_t id) noexcept { _id = id; }
    [[nodiscard]] bool nodeOk() const noexcept;

    // --- pump (только Node) ---

    void tick() noexcept;
    [[nodiscard]] const TxSlot* peekTx() noexcept;
    void onTxResult(bool ok) noexcept;

    // --- Heartbeat ---

    void ping() noexcept;
    void pong() noexcept;
    void onHeartbeat(const msg::Header& hdr) noexcept;
    void hbLost() noexcept;

    // --- Ack window ---

    void abortAck() noexcept;
    void onAck(const msg::Packet& pkt) noexcept;
    void onAckTimeout() noexcept;

    // --- TX внутренности ---

    enum class OutSrc : uint8_t { None, Ctrl, Req };

    bool transmit(const msg::Message& body, uint8_t pkt_id = 0) noexcept;
    void clearTx() noexcept;
    [[nodiscard]] const TxSlot* peekCtrl() noexcept;
    /** @param allow_waiting true — голова при wait (match/abort); false — TX/retry. */
    [[nodiscard]] const TxSlot* peekReq(bool allow_waiting = false) noexcept;

    // --- данные ---

    Node& _node;
    uint8_t _id = 0;
    uint8_t _peer_id = 0;
    uint8_t _pkt_tx = 0;

    Status _status = Status::Idle;
    bool _master = false;
    MISC::ReplyTimer _hb{msg::kHeartbeatMissMax};
    MISC::ReplyTimer _ack{msg::kAckRetryControl};
    OutSrc _out = OutSrc::None;
    bool _rr_ctrl = true;

    TxQueue<SMCP_SESSION_TX_CAPACITY> _tx_req;
    TxQueue<SMCP_SESSION_CTRL_TX_CAPACITY> _tx_ctrl;
};

/** Банк S[N]: ctor регистрирует каждый в Node::sessions() подряд. */
template <std::size_t N, typename S = Session>
class SessionBank {
public:
    template <typename Owner>
    explicit SessionBank(Owner& owner) noexcept
        : SessionBank(owner, std::make_index_sequence<N>{})
    {}

    [[nodiscard]] S& operator[](std::size_t i) noexcept { return _items[i]; }
    [[nodiscard]] const S& operator[](std::size_t i) const noexcept { return _items[i]; }

    [[nodiscard]] S* begin() noexcept { return _items; }
    [[nodiscard]] S* end() noexcept { return _items + N; }
    [[nodiscard]] const S* begin() const noexcept { return _items; }
    [[nodiscard]] const S* end() const noexcept { return _items + N; }

private:
    template <typename Owner, std::size_t... I>
    SessionBank(Owner& owner, std::index_sequence<I...>) noexcept
        : _items{((void)I, S{owner})...}
    {}

    S _items[N];
};

} // namespace smcp
