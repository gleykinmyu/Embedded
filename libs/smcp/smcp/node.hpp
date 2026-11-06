/**
 * @file node.hpp
 * @brief Базовый узел SMCP: ILink + TxQueue + SessionSlots (IPolySlot).
 *
 * Session unicast — Session TX (Full → onTxFull).
 * Node drain сессии + bus; пул — SessionBank&lt;N,S&gt; = PolyStore у leaf.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "mem/polyslot.hpp"
#include "ms_timer.hpp"
#include "ringbuffer.hpp"
#include "smcp/ilink.hpp"
#include "smcp/message.hpp"

namespace smcp {

#ifndef SMCP_TX_QUEUE_CAPACITY
#define SMCP_TX_QUEUE_CAPACITY 16u
#endif

#ifndef SMCP_TX_STALL_MS
#define SMCP_TX_STALL_MS session::Ack::kTimeoutMs
#endif

#ifndef SMCP_MAX_UPDATE_DEPTH
#define SMCP_MAX_UPDATE_DEPTH 3u
#endif

class Session;
class Node;

/** Пул сессий узла: IPolySlot&lt;Session&gt; (leaf — SessionBank / PolyStore). */
using SessionSlots = mem::IPolySlot<Session, uint8_t>;

/** Слот исходящего SMCP до сборки Packet в ILink. */
struct TxSlot {
    Message msg{};
    uint8_t dst_id = Packet::kBroadcastId;
    uint8_t txn_id = 0;
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

/**
 * Узел на шине: ILink, bus `_tx`, SessionSlots.
 * Drain RR: capacity+1 (сессии + bus). RX decode — на ILink.
 */
class Node {
public:
    // --- типы ---

    /** Lifecycle на шине: Idle → Listen → Ready. Отказы — в Error, не здесь. */
    enum class State : uint8_t {
        Idle = 0, /**< До begin() / после end() / hard error. */
        Listen,   /**< id на шине; ждём чужой src == наш. */
        Ready,    /**< Listen прошёл; сессии и TX. */
    };

    [[nodiscard]] static const char* cstr(State state) noexcept
    {
        switch (state) {
        case State::Idle: return "Idle";
        case State::Listen: return "Listen";
        case State::Ready: return "Ready";
        }
        return "?";
    }

    /**
     * Sticky причина (смотреть всегда; hard → State::Idle).
     * TX fail — не сюда: ILink::getStatus() + результат send.
     */
    enum class Error : uint8_t {
        None = 0,
        IdConflict,     /**< на шине кадр с src == наш id → Idle */
        RegisterFailed, /**< нет слота оси в inventory */
    };

    [[nodiscard]] static const char* cstr(Error error) noexcept
    {
        switch (error) {
        case Error::None: return "None";
        case Error::IdConflict: return "IdConflict";
        case Error::RegisterFailed: return "RegisterFailed";
        }
        return "?";
    }

    /**
     * Ребро Session↔peer для onLink (не Node::State).
     * Up — Open; Down — штатный close; HbLost / PeerDown — нештатный down.
     * IdConflict узла — Error + Idle, не Link.
     */
    enum class Link : uint8_t {
        Up = 0,
        Down,     /**< close штатно (begin/end/stop / IdConflict cleanup) */
        HbLost,
        PeerDown, /**< чужой session::Fault на шине, не IdConflict */
    };

    [[nodiscard]] static const char* cstr(Link link) noexcept
    {
        switch (link) {
        case Link::Up: return "Up";
        case Link::Down: return "Down";
        case Link::HbLost: return "HbLost";
        case Link::PeerDown: return "PeerDown";
        }
        return "?";
    }

    using ClockFn = uint32_t (*)();

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

    /** Короткое имя (≤ session::Fault::kDetailMax); уходит в Fault IdConflict.detail. */
    static constexpr uint8_t kNameMax = session::Fault::kDetailMax;
    void setName(const char* node_name) noexcept;
    [[nodiscard]] const char* name() const noexcept { return _name; }

    // --- state / error ---

    [[nodiscard]] State getState() const noexcept { return _state; }
    [[nodiscard]] Error error() const noexcept { return _error; }
    [[nodiscard]] bool isReady() const noexcept { return _state == State::Ready; }
    [[nodiscard]] bool hasError() const noexcept { return _error != Error::None; }
    /** Сброс sticky Error + ILink::clearError (State не меняет). */
    void clearError() noexcept;

    /**
     * Свой id на шине + Listen (~Heartbeat::kTimeoutMs).
     * closeAllSessions(); clearError. update() Listen → Ready.
     */
    void begin(uint8_t node_id) noexcept;
    /** Уйти с шины: closeAllSessions(); clearError → Idle. */
    void end() noexcept;

    // --- сессии (lookup) ---

    [[nodiscard]] uint8_t sessionCapacity() const noexcept
    {
        return const_cast<Node*>(this)->sessions().capacity();
    }
    /** Слот банка по индексу 0…capacity-1 (не поле Session). */
    Session* session(uint8_t slot) noexcept { return sessions().at(slot); }
    const Session* session(uint8_t slot) const noexcept
    {
        return const_cast<Node*>(this)->sessions().at(slot);
    }
    /** Connecting/Awaiting/Open с этим peer; Idle не отдаём (будит openNewSession / start). */
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
    void send(Message msg, uint8_t dst_id = Packet::kBroadcastId, uint8_t txn_id = 0) noexcept;

    template <typename T>
    void send(const T& pdu, uint8_t dst_id = Packet::kBroadcastId, uint8_t txn_id = 0) noexcept
    {
        Message m{};
        if (!pdu.pack(m)) {
            onPackFailed(nullptr, T::kId);
            return;
        }
        send(m, dst_id, txn_id);
    }

    /** RX → acceptRx → session/onPacket → tick → RR Session TX + bus. */
    void update() noexcept;

    /** Глубина вложенного update (TxQueue::enqueue stall). */
    [[nodiscard]] uint8_t updateDepth() const noexcept { return _updateDepth; }

    /** close(@a link) по всем слотам; @a link ≠ Up (обычно Down). Idle — no-op. */
    void closeAllSessions(Link link = Link::Down) noexcept;

    // --- hooks (leaf / app; public — Session / FSM зовут без friend) ---

    /**
     * Demux, если Session кадр не съела.
     * Наследник, если перекрывает — зовёт Node::onPacket.
     */
    virtual void onPacket(const Packet& pkt) noexcept;

    /** Edge Node::State (Idle / Listen / Ready). */
    virtual void onState(State state) noexcept;

    /** Edge sticky Error (в т.ч. None после clearError). Hard: уже Idle. */
    virtual void onError(Error error) noexcept;

    /** Отказ enqueue. nullptr — bus `_tx`. */
    virtual void onTxFull(Session* session) noexcept;

    /**
     * Unicast HB от @a peer_id, а свободной Session (Status::Idle) нет.
     * Не sticky State — событие ёмкости.
     */
    virtual void onSessionFull(uint8_t peer_id) noexcept;

    /**
     * Ребро линка сессии: Up / Down / HbLost / PeerDown.
     * Peer — session->peerId() (после Down close его не стирает).
     * Зовут и при down из Connecting/Awaiting (не только из Open).
     */
    virtual void onLink(Session* session, Link link) noexcept;

    /**
     * Ack на class A.
     * @a req — исходный pending; валиден только до return (drop после хука).
     */
    virtual void onAck(Session* session, const TxSlot& req, const session::Ack& reply) noexcept;

    /**
     * Nack на class A (с шины) или локальный Timeout.
     * @a req валиден только до return (drop после хука).
     */
    virtual void onNack(Session* session, const TxSlot& req, const session::Nack& reply) noexcept;

    /**
     * Diag: PDU не pack. Кадр не в очереди. nullptr — Node::send (bus).
     */
    virtual void onPackFailed(Session* session, uint8_t msg_id) noexcept;

    /**
     * Wire-результат головы bus `_tx`.
     * Default: !ok → drop головы (класс D); ok — уже drop в pumpTx.
     */
    virtual void onTxResult(bool ok) noexcept
    {
        if (!ok) {
            _tx.drop();
        }
    }

protected:
    // --- storage (leaf: SessionBank = PolyStore&lt;Session, S&gt;) ---

    [[nodiscard]] virtual SessionSlots& sessions() noexcept = 0;

    void setState(State state) noexcept;
    void setError(Error error) noexcept;

private:
    // --- RX фильтр / IdConflict ---

    /** src==наш → enterIdConflict; Idle → drop; dst≠нам → drop. */
    [[nodiscard]] bool acceptRx(const Packet& pkt) noexcept;
    /**
     * Свой дубль на шине: broadcast Fault IdConflict, close сессий,
     * drain bus, Error::IdConflict + State::Idle (стоп TX/RX).
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
    State _state = State::Idle;
    Error _error = Error::None;
    ClockFn _clock;
    uint32_t _now_ms = 0;
    uint8_t _updateDepth = 0;
    uint8_t _drainCursor = 0;
    MISC::MsTimer _listen{};
    char _name[kNameMax + 1u]{};
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

        if (node.error() == Node::Error::IdConflict) {
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
