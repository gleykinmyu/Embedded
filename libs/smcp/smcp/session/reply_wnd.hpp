/**
 * @file reply_wnd.hpp
 * @brief FSM окна class A: очередь `_tx_req` + Ack/Nack timeout/retry.
 */

#pragma once

#include <cstdint>

#include "ms_timer.hpp"
#include "smcp/node.hpp"

#ifndef SMCP_SESSION_TX_CAPACITY
#define SMCP_SESSION_TX_CAPACITY 16u
#endif

namespace smcp {

class Session;

/**
 * ReplyWindow: команда → Ack|Nack (голова очереди, окно 1).
 * Только short RPC (Select…). Get/Set — TaggedOps.
 */
class ReplyWnd {
public:
    explicit ReplyWnd(Session& session) noexcept;

    [[nodiscard]] bool enqueue(const TxSlot& item) noexcept;

    /**
     * Сброс очереди + таймера.
     * @a notify и окно ждало Ack — onNack(Body, Abort) по голове; хвост тихо.
     */
    void abort(bool notify = true) noexcept;

    [[nodiscard]] bool isFull() const noexcept { return _q.isFull(); }
    [[nodiscard]] bool isWaiting() const noexcept { return _ack.isWaiting(); }

    /**
     * Голова для TX/retry: nullptr, пока ждём Ack (кроме окна retry по timeout).
     */
    [[nodiscard]] const TxSlot* peek(uint32_t now_ms) noexcept;

    /** После успешного wire головы req — старт/restart `_ack`. */
    void startAck(uint32_t now_ms) noexcept;

    /** Свой Ack|Nack (окно ждёт, txn головы) → Session::ack/nack. */
    [[nodiscard]] bool onPacket(const Packet& pkt) noexcept;

    void tick(uint32_t now_ms) noexcept;

private:
    Session& _s;
    TxQueue<SMCP_SESSION_TX_CAPACITY> _q;
    /* Окно Ack = 1 (один таймер). Шире → несколько in-flight + abort/nack по всем. */
    MISC::ReplyTimer _ack{msg::Ack::kRetry};
};

} // namespace smcp
