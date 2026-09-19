/**
 * @file group_console.hpp
 * @brief IGroupConsole + GroupConsole<N>: IConsole + queued recall.
 *
 * IGroupConsole — билет Select после CGroup::recall (Ack → onGroupAck).
 * GroupConsole<N> — primary Session + registry осей (как Console<N>).
 * CMechBank / Show / Browser — у leaf (MConsole).
 */

#pragma once

#include <cstdint>

#include "smcp/Console/console.hpp"

namespace smcp {

class CGroup;

/** IConsole + очередь recall группы (слот GRUP до Ack/Nack Select). */
class IGroupConsole : public IConsole {
public:
    static constexpr uint8_t kNoQueuedGroup = 0xFFu;

    /** Слот GRUP в полёте; нет — kNoQueuedGroup. */
    [[nodiscard]] uint8_t queuedGroup() const noexcept { return _queuedGroup; }

protected:
    /** @a primary — слот сессии наследника (GroupConsole::_session). */
    explicit IGroupConsole(ILink& link, ClockFn clock, Session& primary) noexcept
        : IConsole(link, clock, primary)
    {}

    /** Ack Select после CGroup::recall(); @a id — слот GRUP. */
    virtual void onGroupAck(uint8_t group_id) noexcept { (void)group_id; }
    /** Queued recall: Select Ack → onGroupAck. UI дописывает поверх. */
    void onAck(Session* session, const TxSlot& req) noexcept override;
    /** Сброс queued recall на Select Nack; UI дописывает разбор reply. */
    void onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept override;

    void clearQueuedGroup() noexcept { _queuedGroup = kNoQueuedGroup; }

private:
    friend class CGroup;
    void setQueuedGroup(uint8_t group_id) noexcept;

    uint8_t _queuedGroup = kNoQueuedGroup;
};

/** GroupConsole<N>: сессии MaxSessions+1 + primary; CMech — leaf. */
template <uint8_t MaxMechs, uint8_t MaxSessions = 1u>
class GroupConsole : public IGroupConsole {
public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kSessionCount = static_cast<uint8_t>(MaxSessions + 1u);

    /** Primary-сессия — _session, слот 0 реестра. */
    explicit GroupConsole(ILink& link, ClockFn clock) noexcept
        : IGroupConsole(link, clock, _session)
        , _session(*this)
    {}

protected:
    [[nodiscard]] MechReg& storage() noexcept override { return _mechs; }
    [[nodiscard]] SessionReg& sessions() noexcept override { return _sessions; }

private:
    /* _sessions до _session: Session ctor → register → sessions(). */
    SessionStore<kSessionCount> _sessions;
    Session _session;
    MechStore<MaxMechs> _mechs;
};

} // namespace smcp
