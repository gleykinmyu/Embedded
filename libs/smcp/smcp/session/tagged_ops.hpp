/**
 * @file tagged_ops.hpp
 * @brief FSM Get/Set/Notify: tagged body через MultiByte, свои таймеры.
 *
 * Не использует ReplyWnd. TX opener + тело → `_tx_ctrl`.
 * Set-клиент после тела ждёт Ack|Nack (_ack).
 * Get: короткий Data и Nack здесь. DataBegin→Data* — MultiByte (armRx в get).
 * Владелец MB: MultiByteClient (onMbBuffer / onMbResult / onMbTxDone).
 */

#pragma once

#include <cstdint>

#include "ms_timer.hpp"
#include "smcp/node.hpp"
#include "smcp/session/multi_byte.hpp"

namespace smcp {

class Session;

class TaggedOps : public MultiByteClient {
public:
    enum class State : uint8_t {
        Idle = 0,
        GetAsk,         ///< свой Get: MB ждёт тело; здесь короткий Data или Nack
        GetReply,       ///< sendBody multi: MB качает TX
        SetIn,          ///< входящий multi Set
        SetOutInline,   ///< короткий свой Set в `_tx_ctrl`
        SetOut,         ///< длинный свой Set: MB качает тело
        SetOutAwaitAck, ///< свой Set: ждём Ack|Nack
    };

    TaggedOps(Session& session, MultiByte& mb) noexcept;

    [[nodiscard]] bool isBusy() const noexcept { return _state != State::Idle; }

    [[nodiscard]] bool set(uint8_t tag, const IBuf& buf) noexcept;
    [[nodiscard]] bool get(uint8_t tag, IBuf& buf) noexcept;

    /**
     * Тело ответа на Get (сервер): short Data или DataBegin→Data*, echo @a txn_id.
     */
    [[nodiscard]] bool sendBody(uint8_t tag, const IBuf& buf, uint8_t txn_id) noexcept;

    /** Get / Set / Ack|Nack / короткий Data своего Get. DataBegin и Data* тела — Session → MultiByte. */
    [[nodiscard]] bool onPacket(const Packet& pkt, uint32_t now_ms) noexcept;

    void tick(uint32_t now_ms) noexcept;

    /** SetOutInline: opener ушёл на wire → SetOutAwaitAck. */
    void onTxResult(const TxSlot& slot) noexcept;

    /** close: GetAsk → onReply; SetOut* → nack; SetIn → onSet. */
    void abort(bool notify = true) noexcept;
    void reset() noexcept;

    /** MultiByteClient: NeedBuf входящего Set → onSetBuffer. */
    [[nodiscard]] bool onMbBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                                  IBuf& buf) noexcept override;
    /** MultiByteClient: RX Done/Fail / Timeout. */
    void onMbResult(uint8_t tag, const IBuf& buf, MultiByte::Error err) noexcept override;
    /** MultiByteClient: TX тело Done → GetReply reset / SetOut: resetXfer + AwaitAck. */
    void onMbTxDone() noexcept override;

private:
    [[nodiscard]] bool isSetOut() const noexcept
    {
        return _state == State::SetOutInline || _state == State::SetOut
            || _state == State::SetOutAwaitAck;
    }

    /** Тег своего Get из opener в `_pend`. */
    [[nodiscard]] uint8_t getTag() const noexcept;
    void failGet(MultiByte::Error err) noexcept;
    void finishShortData(const msg::Data& chunk) noexcept;

    [[nodiscard]] bool takeSetBuf(uint8_t tag, uint8_t txn_id, uint16_t size,
                                  IBuf& buf) noexcept;
    /** onSet + Ack|Nack(Body). @a txn==0 — только onSet (без wire). */
    void finishSetIn(uint8_t tag, const IBuf& buf, MultiByte::Error err,
                     uint8_t txn) noexcept;
    void armPend(const Message& m, uint8_t txn) noexcept;
    void beginAck(uint8_t txn, uint8_t msg_id, uint32_t now_ms) noexcept;

    Session& _s;
    MultiByte& _mb;
    State _state = State::Idle;
    TxSlot _pend{};
    /** SetOutAwaitAck: окно Ack с retry. */
    MISC::ReplyTimer _ack{msg::Ack::kRetry};
};

} // namespace smcp
