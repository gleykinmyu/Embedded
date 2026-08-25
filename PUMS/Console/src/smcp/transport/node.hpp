/**
 * @file node.hpp
 * @brief Базовый узел SMCP: ILink + TxQueue + registry Session*.
 *
 * Session unicast — в Session::_tx (Full → Session::isTxFull, edge Node::onTxFull).
 * Node drain сессии, пока не Idle. Node::send — broadcast / без сессии (Full → onTxFull(nullptr)).
 * Объекты Session создаёт наследник и регистрирует (как IMech).
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
#define SMCP_TX_STALL_MS msg::kAckTimeoutControlMs
#endif

#ifndef SMCP_MAX_UPDATE_DEPTH
#define SMCP_MAX_UPDATE_DEPTH 3u
#endif

class Session;
class Node;

/** Слот исходящего SMCP до сборки Packet в ILink. */
struct TxSlot {
    msg::Message body{};
    uint8_t dst_id = msg::kBroadcastId;
    uint8_t pkt_id = 0;
};

/**
 * Кольцо TxSlot + постановка с ожиданием drain (Node::update).
 * isFull — sticky после неудачного stall/depth; снимается успешным enqueue / clear.
 * enqueue — ниже, после class Node.
 */
template <std::size_t Cap>
class TxQueue {
    static_assert(Cap >= 2u, "TxQueue: Cap >= 2");

public:
    /**
     * @return true — в очереди; false — Full после stall / depth limit / IdConflict abort.
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
/** Регистрация в Node::sessions() (первый свободный слот); fail → Node::Status::RegisterFailed. */
void registerSession(Node& node, Session& session) noexcept;
} // namespace detail

/**
 * Узел на шине: ILink, Node::_tx (bus), Session* registry.
 * Drain: RR по capacity+1 (сессии + bus, по одному кадру).
 * Статусы sticky; RX decode — на ILink, без Node::LinkError.
 */
class Node {
public:
    /**
     * Порядок значений = жёсткость (setStatus не понижает, кроме clearError→OK).
     * OK < LinkError < RegisterFailed < IdConflict.
     */
    enum class Status : uint8_t {
        OK = 0,
        LinkError, /**< Encode / Send / Closed — см. link().getStatus() */
        RegisterFailed, /**< registerSession: нет свободного слота */
        IdConflict, /**< На шине кадр с src == наш node id */
    };

    using ClockFn = uint32_t (*)();

    virtual ~Node() = default;

    explicit Node(ILink& link, ClockFn clock) noexcept;

    /** Время узла (clock из ctor); без часов — 0 / последний кэш. */
    [[nodiscard]] uint32_t clockMs() const noexcept
    {
        return _clock != nullptr ? _clock() : _now_ms;
    }

    uint8_t id() const noexcept { return _link.nodeId(); }
    ILink& link() noexcept { return _link; }
    const ILink& link() const noexcept { return _link; }

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    /** Сброс sticky-статуса → OK (+ link.clearError, onStatus при смене). */
    void clearError() noexcept;

    std::size_t sessionCapacity() const noexcept
    {
        return const_cast<Node*>(this)->sessions().capacity();
    }

    Session* session(uint8_t slot_id) noexcept { return sessions().get(slot_id); }
    const Session* session(uint8_t slot_id) const noexcept
    {
        return const_cast<Node*>(this)->sessions().get(slot_id);
    }

    /** Найти сессию по peer_id (SRC удалённого узла). */
    Session* sessionByPeer(uint8_t peer_id) noexcept;
    const Session* sessionByPeer(uint8_t peer_id) const noexcept
    {
        return const_cast<Node*>(this)->sessionByPeer(peer_id);
    }

    /**
     * Non-session TX (обычно broadcast). Unicast сессий — Session::send.
     * При Full — stall update() до SMCP_TX_STALL_MS, иначе onTxFull(nullptr).
     * LinkError снимается успешным wire или clearError().
     */
    void send(const msg::Message& body,
              uint8_t dst_id = msg::kBroadcastId,
              uint8_t pkt_id = 0) noexcept;

    /**
     * Входной фильтр RX (с первого poll, до/без сессии):
     * src==наш → IdConflict; уже IdConflict → drop; dst≠нам → drop.
     */
    [[nodiscard]] bool acceptRx(const msg::Packet& pkt) noexcept;

    /** Pump: RX → session/app → tick → RR Session::_tx + Node::_tx. */
    void update() noexcept;

    /** Глубина вложенного update (TxQueue::enqueue stall). */
    uint8_t updateDepth() const noexcept { return _updateDepth; }

protected:
    friend void detail::registerSession(Node& node, Session& session) noexcept;
    friend class Session;

    [[nodiscard]] virtual MISC::ObjRegistry<Session, uint8_t>& sessions() noexcept = 0;

    /** Прикладной demux, если сессия кадр не съела. */
    virtual void onPacket(const msg::Packet& pkt) noexcept { (void)pkt; }

    /** Переход Node::Status (edge). Сессии не трогать — только app/UI/диагностика. */
    virtual void onStatus(Status status) noexcept { (void)status; }

    /** Отказ enqueue: очередь Full. nullptr — Node::_tx. */
    virtual void onTxFull(Session* session) noexcept
    {
        (void)session;
    }

    /**
     * Wire-результат головы Node::_tx.
     * По умолчанию drop (класс D, без окна Ack). Session на fail голову держит.
     */
    virtual void onTxResult(bool ok) noexcept
    {
        if (!ok) _tx.drop();
    }

private:
    /** Смена статуса: OK всегда; иначе только «жёстче». Edge → onStatus. */
    void setStatus(Status status) noexcept;

    /** true — wire OK; false — fail (LinkError) или IdConflict (без LinkError). */
    [[nodiscard]] bool sendWire(const TxSlot& item) noexcept;

    void pumpTx(bool do_tick) noexcept;

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
            return false; /* не Full */
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
