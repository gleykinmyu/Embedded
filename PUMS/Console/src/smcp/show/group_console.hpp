/**
 * @file group_console.hpp
 * @brief GroupConsole + CGroup: show::Console + queued recall.
 *
 * Билет Select после CGroup::recall (Ack → onGroupAck).
 * shownGroup — только active (после Ack), не queued.
 * onLink(Down/Lost) — сброс queued+active (без Select).
 * onState(не Ready) — тоже сброс (Idle/Listen).
 * SessionBank и банк осей (CGMech / show::Axis) — у leaf (MConsole).
 */

#pragma once

#include <cstdint>

#include "smcp/show/group.hpp"
#include "smcp/show/console.hpp"

namespace smcp::show {

class IGroupBank;
class GroupConsole;

/**
 * Вид на слот GRUP. Копия — указатель; запись остаётся в секции.
 */
class CGroup {
public:
    CGroup() = default;

    CGroup(Group& rec, IGroupBank& bank, GroupConsole& console) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return _rec != nullptr; }

    [[nodiscard]] uint8_t id() const noexcept;
    [[nodiscard]] const segment::Selection& sel() const noexcept;
    [[nodiscard]] REG::BitMask<Group::Flag> flags() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool contains(uint8_t axis_id) const noexcept;
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
     * Select маски и билет queued group на txn_id этого кадра.
     * Empty / Blocked / OverlapsBlocked / NotSent — отказ.
     */
    [[nodiscard]] Result recall() noexcept;

    /**
     * Записать выделение в слот.
     * Occupied снимается @a confirmed; OverlapsBlocked — нет.
     */
    [[nodiscard]] Result record(segment::Selection selection, const char* name = nullptr,
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
    GroupConsole* _console = nullptr;
};

/** show::Console + очередь recall группы (слот GRUP до Ack/Nack Select). */
class GroupConsole : public Console {
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
    explicit GroupConsole(ILink& link, ClockFn clock, const IPatch* patch = nullptr) noexcept
        : Console(link, clock, patch)
    {}

    /** Ack Select после CGroup::recall(); @a id — слот GRUP. */
    virtual void onGroupAck(uint8_t group_id) noexcept { (void)group_id; }
    void onLink(::smcp::Session* session, Link link) noexcept override;
    void onState(State state) noexcept override;
    void onAck(::smcp::Session* session, const TxSlot& req, const session::Ack& reply) noexcept override;
    void onNack(::smcp::Session* session, const TxSlot& req, const session::Nack& reply) noexcept override;

    void clearQueuedGroup() noexcept
    {
        _queued = CGroup{};
        _queuedTxnId = 0u;
    }

private:
    friend class CGroup;
    /** @a group — вид на запись секции. pkt — кадр, который только что встал в очередь. */
    void setQueuedGroup(CGroup group, Session& session) noexcept;

    CGroup _queued{};
    CGroup _active{};
    uint8_t _queuedTxnId = 0u;
};

/**
 * INVALID — не использовать.
 * Раньше override select гасил queued recall; виртуальный select убран.
 * Слот SessionBank — `show::Session`.
 */
class GServerSession final {
    GServerSession() = delete;
};

} // namespace smcp::show
