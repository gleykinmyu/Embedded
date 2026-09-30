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
 * Вид на слот GRUP: заголовок и массив масок.
 * Длина массива — head->count. Индекс вне count — отказ.
 * Копия — указатели; запись остаётся в секции.
 */
class CGroup {
public:
    CGroup() = default;

    CGroup(GroupHeader* head, Selection* sel,
           IGroupBank* bank = nullptr, IGroupConsole* console = nullptr) noexcept;

    template <uint8_t Segments>
    CGroup(Group<Segments>& rec, IGroupBank& bank, IGroupConsole& console) noexcept
        : CGroup(&rec.head, rec.sel, &bank, &console)
    {}

    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] GroupHeader* header() noexcept;
    [[nodiscard]] const GroupHeader* header() const noexcept;

    [[nodiscard]] Selection* at(uint8_t index) noexcept;
    [[nodiscard]] const Selection* at(uint8_t index) const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool contains(uint8_t index, uint8_t mech_id) const noexcept;
    [[nodiscard]] Selection merged() const noexcept;

    /** Записать маску. Индекс вне count — false. */
    bool setAt(uint8_t index, Selection selection) noexcept;

    [[nodiscard]] uint8_t id() const noexcept;
    [[nodiscard]] REG::BitMask<GroupHeader::Flag> flags() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

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
     * Записать маски сегментов 0…Segments-1 из массива.
     * Occupied снимается @a confirmed; OverlapsBlocked — нет.
     */
    template <uint8_t Segments>
    [[nodiscard]] Result record(const Selection (&selection)[Segments], const char* name = nullptr,
                                bool confirmed = false) noexcept
    {
        static_assert(Segments >= 1u && Segments <= kMaxGroupSegments, "record segments");
        return recordSegments(selection, Segments, name, confirmed);
    }
    /** Переименовать непустую группу. */
    [[nodiscard]] bool rename(const char* name) noexcept;
    /**
     * Очистить слот.
     * Пустая — true; непустая — только @a confirmed.
     */
    [[nodiscard]] bool clear(bool confirmed = false) noexcept;

private:
    [[nodiscard]] Result recordSegments(const Selection* selection, uint8_t count, const char* name,
                                        bool confirmed) noexcept;

    GroupHeader* _head = nullptr;
    Selection* _sel = nullptr;
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
        return _queued ? _queued.header()->id : kNoQueuedGroup;
    }
    /** Слот после Ack Select; нет — kNoActiveGroup. */
    [[nodiscard]] uint8_t activeGroup() const noexcept
    {
        return _active ? _active.header()->id : kNoActiveGroup;
    }
    /** Только active. Queued на кнопку не кладётся. */
    [[nodiscard]] uint8_t shownGroup() const noexcept { return activeGroup(); }
    /** Сброс queued/active и Select Remove маски группы на сессию её сегмента. */
    void clearActiveGroup() noexcept;

protected:
    explicit IGroupConsole(ILink& link, ClockFn clock) noexcept
        : IConsole(link, clock)
    {}

    /** Ack Select после CGroup::recall(); @a id — слот GRUP. */
    virtual void onGroupAck(uint8_t group_id) noexcept { (void)group_id; }
    /** Сессия / свой узел (session == nullptr): сброс queued+active. */
    void onFault(Session* session, Fault reason) noexcept override;
    /** Сессия down: сброс queued+active (в т.ч. close(None)). */
    void onLink(Session* session, bool up) noexcept override;
    /** Idle / Listen / IdConflict / RegisterFailed — сброс queued+active. */
    void onStatus(Status status) noexcept override;
    /** Queued recall: Select Ack → active + onGroupAck. UI дописывает поверх. */
    void onAck(Session* session, const TxSlot& req, const msg::Ack& reply) noexcept override;
    /** Сброс queued recall на Select Nack; UI дописывает разбор reply. */
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

    /** Вид на запись GRUP. Указатели живут, пока жив слот секции. */
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
