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
#include "smcp/transport/ilink.hpp"
#include "smcp/transport/message.hpp"

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
    msg::Message msg{};
    uint8_t dst_id = msg::kBroadcastId;
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

    [[nodiscard]] const TxSlot* peek() const noexcept { return _q.peek(); }
    void drop() noexcept { _q.drop(); }
    void clear() noexcept
    {
        _q.clear();
        _full = false;
    }

    [[nodiscard]] bool isFull() const noexcept { return _full; }
    [[nodiscard]] std::size_t size() const noexcept { return _q.size(); }
    [[nodiscard]] std::size_t space() const noexcept { return _q.space(); }
    [[nodiscard]] bool empty() const noexcept { return _q.empty(); }

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
     * Порядок = жёсткость (setStatus не понижает, кроме clearError→OK).
     * OK < LinkError < RegisterFailed < IdConflict.
     */
    enum class Status : uint8_t {
        OK = 0,
        LinkError,      /**< Encode / Send / Closed — см. link().getStatus() */
        RegisterFailed, /**< нет слота сессии / оси в inventory */
        IdConflict,     /**< на шине кадр с src == наш id */
    };

    [[nodiscard]] static const char* cstr(Status status) noexcept
    {
        switch (status) {
        case Status::OK: return "OK";
        case Status::LinkError: return "LinkError";
        case Status::RegisterFailed: return "RegisterFailed";
        case Status::IdConflict: return "IdConflict";
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
    /** Sticky → OK (+ link.clearError, onStatus). */
    void clearError() noexcept;

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

    // --- TX / pump (app) ---

    /**
     * Non-session TX (обычно broadcast). Unicast — Session::send.
     * Full → stall update() до SMCP_TX_STALL_MS, иначе onTxFull(nullptr).
     */
    void send(msg::Message msg, uint8_t dst_id = msg::kBroadcastId, uint8_t pkt_id = 0) noexcept;

    template <typename T>
    void send(const T& pdu, uint8_t dst_id = msg::kBroadcastId, uint8_t pkt_id = 0) noexcept
    {
        msg::Message m{};
        m.id = T::kId;
        pdu.pack(m);
        send(m, dst_id, pkt_id);
    }

    /** RX → acceptRx → session/onPacket → tick → RR Session TX + bus. */
    void update() noexcept;

    /** Глубина вложенного update (TxQueue::enqueue stall). */
    [[nodiscard]] uint8_t updateDepth() const noexcept { return _updateDepth; }

protected:
    friend class Session;
    friend void detail::registerSession(Node& node, Session& session) noexcept;

    // --- storage (leaf) ---

    [[nodiscard]] virtual SessionReg& sessions() noexcept = 0;

    // --- hooks (leaf / app) ---

    /** Demux, если Session кадр не съела. */
    virtual void onPacket(const msg::Packet& pkt) noexcept { (void)pkt; }

    /**
     * Edge Node::Status. Сессии не трогать — app/UI.
     * IdConflict: TX/RX уже стоп; close — здесь наверху.
     */
    virtual void onStatus(Status status) noexcept;

    /** Отказ enqueue. nullptr — bus `_tx`. */
    virtual void onTxFull(Session* session) noexcept;

    /**
     * Unicast HB от @a peer_id, а свободного Session (peer_id==0) нет.
     * Не sticky Status — событие ёмкости.
     */
    virtual void onSessionFull(uint8_t peer_id) noexcept;

    /**
     * HB потерян (misses ≥ max): сессия ещё up, peer жив; затем Session::close.
     */
    virtual void onHbLost(Session* session) noexcept { (void)session; }

    /**
     * Ack на class A.
     * @a req — исходный pending (голова `_tx_req`); валиден только до return
     * (Session делает drop после хука).
     */
    virtual void onAck(Session* session, const TxSlot& req) noexcept;

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

    /** Смена `_status`; no-op / не понижает (кроме OK через clearError). Лог в onStatus. */
    void setStatus(Status status) noexcept;

private:
    /** src==наш → IdConflict; IdConflict → drop; dst≠нам → drop. */
    [[nodiscard]] bool acceptRx(const msg::Packet& pkt) noexcept;

    // --- TX внутренности ---

    [[nodiscard]] bool sendWire(const TxSlot& item) noexcept;
    void pumpTx(bool do_tick) noexcept;
    /** Unicast HB → первый слот peer_id==0 (bind в Session::onHeartbeat). */
    [[nodiscard]] Session* openNewSession(const msg::Packet& pkt) noexcept;

    // --- данные ---

    ILink& _link;
    TxQueue<SMCP_TX_QUEUE_CAPACITY> _tx;
    Status _status = Status::OK;
    ClockFn _clock;
    uint32_t _now_ms = 0;
    uint8_t _updateDepth = 0;
    uint8_t _drainCursor = 0;
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
