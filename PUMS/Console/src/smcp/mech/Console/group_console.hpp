/**
 * @file group_console.hpp
 * @brief IGroupConsole + GroupConsole<N>: IConsole + queued recall.
 *
 * IGroupConsole — билет Select после CGroup::recall (Ack → onGroupAck).
 * shownGroup — только active (после Ack), не queued.
 * Session::onFault(HbLost / IdConflict) — сброс queued+active (без Select).
 * close(None): сброс по onLink(false) / Status Idle·Listen·IdConflict·RegisterFailed.
 * GroupConsole<N> — registry MaxSessions слотов (как Console<N>).
 * GServerSession::select гасит билет до send. recall ставит его после успеха.
 * SessionBank, банк осей и CMechBank — у leaf (MConsole).
 */

#pragma once

#include <cstdint>

#include "smcp/mech/Console/console.hpp"
#include "smcp/mech/group.hpp"

namespace smcp {

class IGroupBank;
class IGroupConsole;

/**
 * Вид на слот GRUP. Копия — указатель; запись остаётся в секции.
 */
class CGroup {
public:
    CGroup() = default;

    CGroup(Group& rec, IGroupBank& bank, IGroupConsole& console) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return _rec != nullptr; }

    [[nodiscard]] uint8_t id() const noexcept;
    [[nodiscard]] const Selection& mech() const noexcept;
    [[nodiscard]] REG::BitMask<Group::Flag> flags() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool contains(uint8_t mech_id) const noexcept;
    [[nodiscard]] bool isBlocked() const noexcept;
    /** Ставит Blocked. При @a on: overlap (инфо); если это queued/active — сброс группы. */
    void setBlocked(bool on = true) noexcept;

    enum class Result : uint8_t {
        Ok = 0,
        Empty,           /**< Пустая маска / слот. */
        Occupied,        /**< Слот непустой, нужен confirmed. */
        Blocked,         /**< Сама группа blocked. */
        OverlapsBlocked, /**< Пересечение с другой заблокированной группой. */
        NotSent,         /**< Select не встал в очередь сессии. */
    };

    /**
     * Select маски и билет queued group на pkt_id этого кадра.
     * Empty / Blocked / OverlapsBlocked / NotSent — отказ.
     */
    [[nodiscard]] Result recall() noexcept;

    /**
     * Записать выделение в слот.
     * Occupied снимается @a confirmed; OverlapsBlocked — нет.
     */
    [[nodiscard]] Result record(Selection selection, const char* name = nullptr,
                                bool confirmed = false) noexcept;
    /** Переименовать непустую группу. */
    [[nodiscard]] bool rename(const char* name) noexcept;
    /**
     * Очистить слот.
     * Пустая — true; непустая — только @a confirmed.
     */
    [[nodiscard]] bool clear(bool confirmed = false) noexcept;

private:
    Group* _rec = nullptr;
    IGroupBank* _bank = nullptr;
    IGroupConsole* _console = nullptr;
};

/** IConsole + очередь recall группы (слот GRUP до Ack/Nack Select). */
class IGroupConsole : public IConsole {
public:
    static constexpr uint8_t kNoQueuedGroup = 0xFFu;
    static constexpr uint8_t kNoActiveGroup = kNoQueuedGroup;

    /** Слот GRUP в полёте; нет — kNoQueuedGroup. */
    [[nodiscard]] uint8_t queuedGroup() const noexcept
    {
        return _queued ? _queued.id() : kNoQueuedGroup;
    }
    /** Слот после Ack Select; нет — kNoActiveGroup. */
    [[nodiscard]] uint8_t activeGroup() const noexcept
    {
        return _active ? _active.id() : kNoActiveGroup;
    }
    /** Только active. Queued на кнопку не кладётся. */
    [[nodiscard]] uint8_t shownGroup() const noexcept { return activeGroup(); }
    /** Сброс queued/active и Select Remove маски группы. */
    void clearActiveGroup() noexcept;

protected:
    explicit IGroupConsole(ILink& link, ClockFn clock) noexcept
        : IConsole(link, clock)
    {}

    /** Ack Select после CGroup::recall(); @a id — слот GRUP. */
    virtual void onGroupAck(uint8_t group_id) noexcept { (void)group_id; }
    void onFault(Session* session, Fault reason) noexcept override;
    void onLink(Session* session, bool up) noexcept override;
    void onStatus(Status status) noexcept override;
    void onAck(Session* session, const TxSlot& req, const msg::Ack& reply) noexcept override;
    void onNack(Session* session, const TxSlot& req, const msg::Nack& reply) noexcept override;

    void clearQueuedGroup() noexcept
    {
        _queued = CGroup{};
        _queuedPkt = 0u;
    }

private:
    friend class CGroup;
    friend class GServerSession;
    /** @a group — вид на запись секции. pkt — кадр, который только что встал в очередь. */
    void setQueuedGroup(CGroup group, ServerSession& session) noexcept;

    CGroup _queued{};
    CGroup _active{};
    uint8_t _queuedPkt = 0u;
};

/**
 * Сессия пульта с группами: Select гасит билет recall до send.
 * Слот SessionBank у leaf.
 */
class GServerSession : public ServerSession {
public:
    explicit GServerSession(IGroupConsole& console) noexcept;

    bool select(msg::Action action, Selection selection) noexcept override;

private:
    IGroupConsole& _group;
};

/** GroupConsole<N>: registry сессий MaxSessions. Банк осей — storage() у leaf. */
template <uint8_t MaxMechs, uint8_t MaxSessions = 1u>
class GroupConsole : public IGroupConsole {
public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kSessionCount = MaxSessions;

    explicit GroupConsole(ILink& link, ClockFn clock) noexcept
        : IGroupConsole(link, clock)
    {}

protected:
    [[nodiscard]] SessionReg& sessions() noexcept override { return _sessions; }

private:
    SessionStore<kSessionCount> _sessions;
};

} // namespace smcp
