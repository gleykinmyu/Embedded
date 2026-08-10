/**
 * @file node.hpp
 * @brief Базовый узел SMCP: Link + TX-очередь + registry Session*.
 *
 * Объекты Session создаёт наследник и регистрирует (как IMech).
 */

#pragma once

#include <cstdint>

#include "ican.hpp"
#include "ms_timer.hpp"
#include "obj_registry.hpp"
#include "ringbuffer.hpp"
#include "smcp/transport/link.hpp"
#include "smcp/transport/message.hpp"
#include "smcp/transport/session.hpp"

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

/** Элемент TX-очереди Node. */
struct Outbound {
    msg::Message body{};
    uint8_t dst_id = msg::kBroadcastId;
    uint8_t pkt_id = 0;
};

class Node;

namespace detail {
/** Регистрация в Node::sessions(); fail → Node::Status::RegisterFailed. */
void registerSession(Node& node, Session& session, uint8_t slot_id) noexcept;
} // namespace detail

/**
 * Узел на шине: Link, общая TX-очередь, таблица Session* (слоты у наследника).
 * send() без bool; отказ → Status. При полной очереди — stall + update() до таймаута.
 */
class Node {
public:
    enum class Status : uint8_t {
        OK = 0,
        TxQueueFull,
        LinkError, /**< Encode / CanSend / CanClosed — см. link().getStatus() */
        IdConflict, /**< На шине кадр с src == наш node id */
        RegisterFailed, /**< registerSession: слот занят / id вне диапазона */
    };

    using ClockFn = uint32_t (*)();

    virtual ~Node() = default;

    explicit Node(BIF::CAN::ICAN& can, uint8_t node_id) noexcept;

    /** Живые ms для stall в send() (как nex::Application::nowMs). */
    void setClock(ClockFn fn) noexcept { _clock = fn; }

    /** Время узла (setClock); без часов — 0 / последний кэш. */
    [[nodiscard]] uint32_t clockMs() const noexcept
    {
        return _clock != nullptr ? _clock() : _now_ms;
    }

    [[nodiscard]] uint8_t id() const noexcept { return _link.nodeId(); }
    [[nodiscard]] Link& link() noexcept { return _link; }
    [[nodiscard]] const Link& link() const noexcept { return _link; }

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::OK; }

    [[nodiscard]] std::size_t sessionCapacity() const noexcept
    {
        return const_cast<Node*>(this)->sessions().capacity();
    }

    [[nodiscard]] Session* session(uint8_t slot_id) noexcept { return sessions().get(slot_id); }
    [[nodiscard]] const Session* session(uint8_t slot_id) const noexcept
    {
        return const_cast<Node*>(this)->sessions().get(slot_id);
    }

    /** Найти сессию по peer_id (SRC удалённого узла). */
    [[nodiscard]] Session* sessionByPeer(uint8_t peer_id) noexcept;
    [[nodiscard]] const Session* sessionByPeer(uint8_t peer_id) const noexcept
    {
        return const_cast<Node*>(this)->sessionByPeer(peer_id);
    }

    /**
     * Поставить в TX-очередь (broadcast по умолчанию).
     * При Full — крутит update() до SMCP_TX_STALL_MS (нужен setClock), иначе TxQueueFull.
     */
    void send(const msg::Message& body,
              uint8_t dst_id = msg::kBroadcastId,
              uint8_t pkt_id = 0) noexcept;

    [[nodiscard]] bool receive(msg::Packet& out) noexcept;

    /**
     * Входной фильтр RX (с первого poll, до/без сессии):
     * src==наш → IdConflict + stop sessions; dst≠нам → drop.
     */
    [[nodiscard]] bool acceptRx(const msg::Packet& pkt) noexcept;

    /** Pump: RX → session/app → tick сессий → drain TX. Время — clockMs() (нужен setClock). */
    void update() noexcept;

protected:
    friend void detail::registerSession(Node& node, Session& session, uint8_t slot_id) noexcept;

    [[nodiscard]] virtual MISC::ObjRegistry<Session, uint8_t>& sessions() noexcept = 0;

    /** Прикладной demux, если сессия кадр не съела. */
    virtual void onPacket(const msg::Packet& pkt) noexcept { (void)pkt; }

private:
    Link _link;
    MISC::RingBuffer<Outbound, SMCP_TX_QUEUE_CAPACITY> _tx;
    Status _status = Status::OK;
    ClockFn _clock = nullptr;
    uint32_t _now_ms = 0;
    uint8_t _updateDepth = 0;
};

} // namespace smcp
