/**
 * @file node.hpp
 * @brief Базовый узел SMCP: ILink + TxQueue + registry Session*.
 *
 * Session unicast — Session TX (Full → isTxFull / onTxFull).
 * Node drain сессии + bus; Session создаёт наследник и регистрирует.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ms_timer.hpp"
#include "obj_registry.hpp"
#include "ringbuffer.hpp"
#include "smcp/ilink.hpp"
#include "smcp/message.hpp"

namespace smcp {

#ifndef SMCP_TX_QUEUE_CAPACITY
#define SMCP_TX_QUEUE_CAPACITY 16u
#endif

#ifndef SMCP_TX_STALL_MS
#define SMCP_TX_STALL_MS msg::Ack::kTimeoutMs
#endif

#ifndef SMCP_MAX_UPDATE_DEPTH
#define SMCP_MAX_UPDATE_DEPTH 3u
#endif

class Session;
class Node;

/** Слот исходящего SMCP до сборки Packet в ILink. */
struct TxSlot {
    Message msg{};
    uint8_t dst_id = Packet::kBroadcastId;
    uint8_t pkt_id = 0;
    bool needs_ack = false;
};

/**
 * Кольцо TxSlot + постановка с ожиданием drain (Node::update).
 * isFull — sticky после stall/depth; снимается успешным enqueue / clear.
 */
template <std::size_t Cap>
class TxQueue {
    static_assert(Cap >= 2u, "TxQueue: Cap >= 2");

public:
    /**
     * @return true — в очереди; false — Full после stall / depth / IdConflict abort.
     */
    [[nodiscard]] bool enqueue(const TxSlot& item, Node& node) noexcept;

    [[nodiscard]] const TxSlot* peek() const noexcept;
    void drop() noexcept;
    void clear() noexcept;

    [[nodiscard]] bool isFull() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t space() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    MISC::RingBuffer<TxSlot, Cap> _q;
    bool _full = false;
};

namespace detail {
/** Регистрация в Node::sessions(); fail → RegisterFailed. */
void registerSession(Node& node, Session& session) noexcept;
} // namespace detail

/**
 * Узел на шине: ILink, bus `_tx`, Session* registry.
 * Drain RR: capacity+1 (сессии + bus). RX decode — на ILink.
 */
class Node {
public:
    /**
     * Lifecycle: Idle → Listen → Ready.
     * Отказ не понижаем, кроме Idle (clearError / end / begin).
     * LinkError → Ready после успешного sendWire.
     */
    enum class Status : uint8_t {
        Idle = 0,       /**< До begin() / после end(). */
        Listen,         /**< id на шине; ждём чужой src == наш. */
        Ready,          /**< Listen прошёл; сессии и TX. */
        LinkError,      /**< Encode / Send / Closed — см. link().getStatus() */
        RegisterFailed, /**< нет слота сессии / оси в inventory */
        IdConflict,     /**< на шине кадр с src == наш id */
    };

    [[nodiscard]] static const char* cstr(Status status) noexcept
    {
        switch (status) {
        case Status::Idle: return "Idle";
        case Status::Listen: return "Listen";
        case Status::Ready: return "Ready";
        case Status::LinkError: return "LinkError";
        case Status::RegisterFailed: return "RegisterFailed";
        case Status::IdConflict: return "IdConflict";
        }
        return "?";
    }

    enum class Fault : uint8_t {
        None = 0, /**< штатное закрытие (begin/end/stop), не отказ шины */
        HbLost,
        IdConflict,
    };

    [[nodiscard]] static const char* cstr(Fault reason) noexcept
    {
        switch (reason) {
        case Fault::None: return "None";
        case Fault::HbLost: return "HbLost";
        case Fault::IdConflict: return "IdConflict";
        }
        return "?";
    }

    using ClockFn = uint32_t (*)();
    using SessionReg = MISC::ObjRegistry<Session, uint8_t>;
    template <uint8_t Cap>
    using SessionStore = MISC::ObjStorage<Session, Cap, uint8_t, 0>;

    // --- ctor ---

    virtual ~Node() = default;
    explicit Node(ILink& link, ClockFn clock) noexcept;

    // --- идентичность / часы / линк ---

    [[nodiscard]] uint8_t id() const noexcept { return _link.nodeId(); }
    [[nodiscard]] uint32_t clockMs() const noexcept
    {
        return _clock != nullptr ? _clock() : _now_ms;
    }
    ILink& link() noexcept { return _link; }
    const ILink& link() const noexcept { return _link; }

    // --- статус узла ---

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    [[nodiscard]] bool isReady() const noexcept { return _status == Status::Ready; }
    [[nodiscard]] bool isFault() const noexcept { return _status >= Status::LinkError; }
    /** Sticky отказ → Idle (+ link.clearError, onStatus). */
    void clearError() noexcept;

    /**
     * Свой id на шине + Listen (~Heartbeat::kTimeoutMs).
     * closeAllSessions(None); отказ → clearError. update() Listen → Ready.
     */
    void begin(uint8_t node_id) noexcept;
    /** Уйти с шины: closeAllSessions(None) → Idle. */
    void end() noexcept;

    // --- сессии (lookup) ---

    [[nodiscard]] uint8_t sessionCapacity() const noexcept
    {
        return static_cast<uint8_t>(const_cast<Node*>(this)->sessions().capacity());
    }
    Session* session(uint8_t slot_id) noexcept { return sessions().get(slot_id); }
    const Session* session(uint8_t slot_id) const noexcept
    {
        return const_cast<Node*>(this)->sessions().get(slot_id);
    }
    Session* sessionByPeer(uint8_t peer_id) noexcept;
    const Session* sessionByPeer(uint8_t peer_id) const noexcept
    {
        return const_cast<Node*>(this)->sessionByPeer(peer_id);
    }
    /** Первый слот Session::Status::Idle; нет — nullptr. */
    [[nodiscard]] Session* idleSession() noexcept;
    [[nodiscard]] const Session* idleSession() const noexcept
    {
        return const_cast<Node*>(this)->idleSession();
    }

    // --- TX / pump (app) ---

    /**
     * Non-session TX (обычно broadcast). Unicast — Session::send.
     * Full → stall update() до SMCP_TX_STALL_MS, иначе onTxFull(nullptr).
     */
    void send(Message msg, uint8_t dst_id = Packet::kBroadcastId, uint8_t pkt_id = 0) noexcept;

    template <typename T>
    void send(const T& pdu, uint8_t dst_id = Packet::kBroadcastId, uint8_t pkt_id = 0) noexcept
    {
        Message m{};
        m.id = T::kId;
        if (!pdu.pack(m)) {
            onPackFailed(nullptr, T::kId);
            return;
        }
        send(m, dst_id, pkt_id);
    }

    /** RX → acceptRx → session/onPacket → tick → RR Session TX + bus. */
    void update() noexcept;

    /** Глубина вложенного update (TxQueue::enqueue stall). */
    [[nodiscard]] uint8_t updateDepth() const noexcept { return _updateDepth; }

    /** close(@a reason) по всем слотам. Idle — no-op. */
    void closeAllSessions(Fault reason) noexcept;

protected:
    friend class Session;
    friend void detail::registerSession(Node& node, Session& session) noexcept;

    // --- storage (leaf) ---

    [[nodiscard]] virtual SessionReg& sessions() noexcept = 0;

    // --- hooks (leaf / app) ---

    /**
     * Demux, если Session кадр не съела.
     * Fault IdConflict: Session::fault. Наследник, если перекрывает — зовёт Node::onPacket.
     */
    virtual void onPacket(const Packet& pkt) noexcept;

    /**
     * Edge Node::Status (Idle/Listen/Ready и отказы).
     * IdConflict: TX/RX стоп; разбор приложения — onFault(nullptr).
     */
    virtual void onStatus(Status status) noexcept;

    /** Отказ enqueue. nullptr — bus `_tx`. */
    virtual void onTxFull(Session* session) noexcept;

    /**
     * Unicast HB от @a peer_id, а свободной Session (Status::Idle) нет.
     * Не sticky Status — событие ёмкости.
     */
    virtual void onSessionFull(uint8_t peer_id) noexcept;

    /**
     * @a session != nullptr — отказ сессии (HbLost / IdConflict), peer жив.
     * @a session == nullptr — свой отказ узла (сейчас IdConflict).
     * close(None) хук не зовёт.
     */
    virtual void onFault(Session* session, Fault reason) noexcept;

    /**
     * Ребро Session::Open. Peer — session->peerId(): close его не стирает.
     * begin/end/stop — close(None), только этот хук (без onFault).
     */
    virtual void onLink(Session* session, bool up) noexcept;

    /**
     * Ack на class A.
     * @a req — исходный pending (голова `_tx_req`); валиден только до return
     * (Session делает drop после хука). @a reply — разобранный Ack.
     */
    virtual void onAck(Session* session, const TxSlot& req, const msg::Ack& reply) noexcept;

    /**
     * Nack на class A (с шины) или локальный Timeout.
     * @a req валиден только до return (Session делает drop после хука).
     */
    virtual void onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept;

    /**
     * Diag: Ack/Nack pkt_id ≠ pending. Сессию не трогаем.
     */
    virtual void onPktIdMismatch(Session* session, uint8_t expected, uint8_t got) noexcept;

    /**
     * Diag: PDU не pack. Кадр не в очереди. nullptr — Node::send (bus).
     */
    virtual void onPackFailed(Session* session, uint8_t msg_id) noexcept;

    /**
     * Wire-результат головы bus `_tx` (очередь без peer: Node::send / broadcast).
     * Не Session TX — там Session::onTxResult.
     * Default: !ok → drop головы (класс D); ok — уже drop в pumpTx.
     */
    virtual void onTxResult(bool ok) noexcept
    {
        if (!ok) {
            _tx.drop();
        }
    }

    // --- статус / RX фильтр ---

    /** Смена `_status`. Отказ не понижаем, кроме Idle и LinkError→Ready. */
    void setStatus(Status status) noexcept;

private:
    /** src==наш → enterIdConflict; IdConflict → drop; dst≠нам → drop. */
    [[nodiscard]] bool acceptRx(const Packet& pkt) noexcept;
    /**
     * Свой дубль на шине: broadcast Fault IdConflict, close всех
     * сессий, drain bus, затем sticky IdConflict (стоп TX/RX).
     */
    void enterIdConflict() noexcept;

    // --- TX внутренности ---

    [[nodiscard]] bool sendWire(const TxSlot& item) noexcept;
    void pumpTx(bool do_tick) noexcept;
    /** Unicast HB → idleSession() (bind в Session::onHeartbeat). */
    [[nodiscard]] Session* openNewSession(const Packet& pkt) noexcept;

    // --- данные ---

    ILink& _link;
    TxQueue<SMCP_TX_QUEUE_CAPACITY> _tx;
    Status _status = Status::Idle;
    ClockFn _clock;
    uint32_t _now_ms = 0;
    uint8_t _updateDepth = 0;
    uint8_t _drainCursor = 0;
    MISC::MsTimer _listen{};
};

template <std::size_t Cap>
bool TxQueue<Cap>::enqueue(const TxSlot& item, Node& node) noexcept
{
    if (_q.push(item)) {
        _full = false;
        return true;
    }

    if (node.updateDepth() >= SMCP_MAX_UPDATE_DEPTH) {
        _full = true;
        return false;
    }

    MISC::MsTimer stall{};
    stall.start(node.clockMs(), SMCP_TX_STALL_MS);

    for (;;) {
        const std::size_t space_before = _q.space();
        node.update();

        if (node.getStatus() == Node::Status::IdConflict) {
            return false;
        }

        if (_q.push(item)) {
            _full = false;
            return true;
        }

        if (_q.space() > space_before) {
            stall.start(node.clockMs(), SMCP_TX_STALL_MS);
        }

        if (stall.timedOut(node.clockMs())) {
            break;
        }
    }

    _full = true;
    return false;
}

} // namespace smcp
