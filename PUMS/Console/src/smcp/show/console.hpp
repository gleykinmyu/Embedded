/**
 * @file console.hpp
 * @brief show::Axis + Console + Session: шоу-слой поверх segment::console.
 *
 * Axis — PatchRec + AxisType* + ProfileSlot* + scale Δ; position()/setTarget — ед. консоли.
 * Telemetry: Console → Session (слоты этого сервера) → Axis*.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "mem/range_alloc.hpp"
#include "segment/console.hpp"
#include "smcp/show/patch.hpp"

namespace smcp {
namespace show {

class Console;
class Axis;

namespace detail {

/** 10^0…10^4; |d| у UnitScale ≤ 4. Saturate к limits&lt;T&gt;. */
template <typename T>
[[nodiscard]] constexpr T applyScale(T v, int8_t d) noexcept
{
    static_assert(std::is_integral_v<T>, "show::detail::applyScale: integral");
    if (d == 0 || v == T{0}) {
        return v;
    }
    constexpr int32_t kPow10[] = {1, 10, 100, 1000, 10000};
    const uint8_t n = static_cast<uint8_t>(d > 0 ? d : -d);
    if (n >= sizeof(kPow10) / sizeof(kPow10[0])) {
        return v;
    }
    const int32_t f = kPow10[n];
    if constexpr (std::is_signed_v<T>) {
        using Wide = int32_t;
        if (d > 0) {
            if (v > 0 && static_cast<Wide>(v) > (std::numeric_limits<T>::max() / f)) {
                return std::numeric_limits<T>::max();
            }
            if (v < 0 && static_cast<Wide>(v) < (std::numeric_limits<T>::min() / f)) {
                return std::numeric_limits<T>::min();
            }
            return static_cast<T>(static_cast<Wide>(v) * f);
        }
        return static_cast<T>(static_cast<Wide>(v) / f);
    } else {
        using Wide = uint32_t;
        if (d > 0) {
            const Wide out = static_cast<Wide>(v) * static_cast<Wide>(f);
            const Wide lim = static_cast<Wide>(std::numeric_limits<T>::max());
            return out > lim ? std::numeric_limits<T>::max() : static_cast<T>(out);
        }
        return static_cast<T>(static_cast<Wide>(v) / static_cast<Wide>(f));
    }
}

} // namespace detail

/**
 * Состояние выборки AxisProfile на сессии.
 * Idle/Fetching — внутренняя FSM; наружу (onAxisProfile) — Ready / *Failed.
 */
enum class FetchState : uint8_t {
    Idle = 0,     /**< Нет применённой маски / после release. */
    Fetching,     /**< Слоты выделены, Get паспортов в процессе. */
    Ready,        /**< Все паспорта собраны (в т.ч. пустая маска). */
    AllocFailed,  /**< pool не дал диапазон под маску. */
    FetchFailed,  /**< Get исчерпал попытки (axis_id в хуке). */
};

/**
 * Результат map PatchRec → Axis ↔ Profile (не axis::Status шины).
 * Idle/Patched — в процессе; остальное — терминал (счётчик → onAxisPatch).
 */
enum class PatchState : uint8_t {
    Idle = 0,       /**< Нет записи APCH на этой оси. */
    Unpatched,      /**< logical_id есть, route пуст. */
    Patched,        /**< APCH+route ок; ждём Open / Fetch Ready. */
    Synced,         /**< Profile привязан (готово к setTarget / Telemetry). */
    NoSession,      /**< start(server) не дал сессию. */
    NoProfile,      /**< Ready, но слота/паспорта нет. */
    KindMismatch,   /**< type.kind ≠ profile.kind; patch остаётся. */
    RouteConflict,  /**< ProfileSlot этого route уже на другой Axis. */
};

/** Терминальный PatchState (ось учтена в прогрессе remap). */
[[nodiscard]] constexpr bool isPatchResolved(PatchState s) noexcept
{
    return s != PatchState::Idle && s != PatchState::Patched;
}

/** Дефолтный Patch на консоли (ёмкости kAxisTypeCount / kPatchCount). */

/**
 * Слот кэша Profile: axis — demux Telemetry; ключ — profile.axis_id.
 * Паспорт «ещё не Get» — Profile::markDirty(id) → !isValid().
 */
struct ProfileSlot {
    Axis* axis = nullptr; /**< После bind / Axis::bindProfile. */
    axis::Profile profile{};

    /** Пишет axis = @a a; у оси — bindProfile(this). @return false — уже эта ось. */
    [[nodiscard]] bool bind(Axis& a) noexcept;
    /** Снять ось (если была). */
    void unbind() noexcept;
    /** Если axis привязан — Axis::refreshScale() (после Get / markDirty). */
    void refreshAxis() noexcept;
};

/** Диапазон ProfileSlot одного сервера (хендл RangeAllocator, SizeT = uint8_t). */
using ProfileRange = mem::AllocRange<ProfileSlot, uint8_t>;
/** Общий аллокатор диапазонов ProfileSlot на show-консоли. */
using ProfileAllocator = mem::IRangeAllocator<ProfileSlot, uint8_t>;

/**
 * Ось пульта: PatchRec (APCH) + тип (ATYP) + ProfileSlot + мост шкалы.
 * Правка APCH — через шоуфайл / PatchSection, не здесь.
 * position() / setTarget — ед. консоли (AxisType.scale); на шину — Profile.scale.
 */
class Axis : public segment::console::Axis {
public:
    explicit Axis(segment::Console& console) noexcept
        : segment::console::Axis(console)
    {}

    /**
     * APCH + тип из ATYP за один шаг: `bindPatch(rec, patch.typeOf(*rec))`.
     * rec == nullptr — unbind (тип тоже сбрасывается).
     * type == nullptr — без типа → шкала = Profile.scale, d=0.
     */
    void bindPatch(const PatchRec* rec, const AxisType* type = nullptr) noexcept;
    /**
     * Привязать слот кэша сессии (nullptr = unbind).
     * Пишет slot->axis = this и берёт profile из слота.
     */
    void bindProfile(ProfileSlot* slot) noexcept;
    /** Unbind profile/patch, PatchState::Idle. */
    void reset() noexcept;

    [[nodiscard]] const PatchRec* patch() const noexcept { return _patch; }
    [[nodiscard]] const AxisType* type() const noexcept { return _type; }
    [[nodiscard]] ProfileSlot* profileSlot() const noexcept { return _slot; }
    [[nodiscard]] const axis::Profile* profile() const noexcept
    {
        return (_slot != nullptr) ? &_slot->profile : nullptr;
    }

    /** Логический id шоу (PatchRec.id); нет патча — kIdNone. */
    [[nodiscard]] uint16_t localId() const noexcept;

    /** Profile + согласованный kind; иначе setTarget → false, Telemetry без моста. */
    [[nodiscard]] bool scaleReady() const noexcept { return _scale_ok; }
    /** d = p_srv − p_con; con→srv: ×10^d, srv→con: ×10^(−d). */
    [[nodiscard]] int8_t scaleDelta() const noexcept { return _scale_d; }

    /** pos (i32) / speed (u16): ×10^d. */
    template <typename T>
    [[nodiscard]] T toServer(T console_value) const noexcept
    {
        return detail::applyScale(console_value, _scale_d);
    }
    /** pos (i32) / speed (u16): ×10^(−d). */
    template <typename T>
    [[nodiscard]] T toConsole(T server_value) const noexcept
    {
        return detail::applyScale(server_value, static_cast<int8_t>(-_scale_d));
    }

    [[nodiscard]] axis::Route route() const noexcept override;

    /**
     * @a target в ед. консоли (Absolute: +pos_offset, Invert).
     * На шину — ед. сервера.
     */
    [[nodiscard]] bool setTarget(const axis::MotionTarget& target) noexcept override;

    /** position() после вызова — ед. консоли (−offset, Invert). */
    void onTelemetry(const segment::Telemetry& telemetry) noexcept override;

    /** Пересчёт scale Δ из текущего profile() / type (после Get или смены patch). */
    void refreshScale() noexcept;

    [[nodiscard]] PatchState patchState() const noexcept { return _patch_state; }
    void setPatchState(PatchState s) noexcept { _patch_state = s; }

private:
    friend class Console;

    const PatchRec* _patch = nullptr;
    const AxisType* _type = nullptr;
    ProfileSlot* _slot = nullptr;
    PatchState _patch_state = PatchState::Idle;
    int8_t _scale_d = 0;
    bool _scale_ok = false;
};

/**
 * segment::console::Session + кэш Profile в диапазоне аллокатора Console.
 * Telemetry этого peer — по слотам (не по всему inventory консоли).
 */
class Session : public segment::console::Session {
public:
    /** Сколько сбоев Get AxisProfile (onReply) до FetchFailed. Busy begin не считает. */
    static constexpr uint8_t kFetchFailLimit = 3u;

    explicit Session(Console& console) noexcept;

    [[nodiscard]] Console& console() noexcept;

    /** Profile по phys axis_id (поиск в диапазоне); нет — nullptr. */
    [[nodiscard]] const axis::Profile* profile(uint8_t axis_id) const noexcept;
    /** Слот по phys axis_id; нет в AxisRegistered — nullptr. */
    [[nodiscard]] ProfileSlot* findSlot(uint8_t axis_id) noexcept;
    [[nodiscard]] const ProfileSlot* findSlot(uint8_t axis_id) const noexcept;

    /** free диапазона + сброс mapped; unbind Axis на слотах. */
    void releaseProfiles() noexcept;

    /** Get AxisRegistered (Link::Up / revision). One-shot; Open обязателен. */
    void requestRegistered() noexcept;

    /**
     * Get AxisProfile(axis_id): слот из AxisRegistered + fetch в slot.
     * Нет слота — Fail. Busy — Busy (retry).
     */
    [[nodiscard]] session::TaggedOps::Result fetchAxisProfile(uint8_t axis_id) noexcept;

    /**
     * Оси консоли с route на этот peer → tryBindProfile.
     * Звать при FetchState::Ready (onAxisProfile / догон remap).
     */
    void bindProfiles() noexcept;

    [[nodiscard]] FetchState fetchState() const noexcept { return _fetch_state; }

    /** Слот по profile.axis_id → Axis::onTelemetry (нужен уже bindProfile). */
    [[nodiscard]] bool onTelemetry(const segment::Telemetry& body) noexcept override;

protected:
    void tick() noexcept override;
    void onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                 session::MultiByte::Error status) noexcept override;

private:
    /**
     * После AxisRegistered: drop + alloc под маску + pending-слоты (Get докачает tick).
     */
    void applyRegistered() noexcept;
    /** Unbind + free диапазона; _axis_registered / _mapped не трогаем. */
    void dropProfileRange() noexcept;
    /** Get в @a slot; при Ok запоминает _fetch_slot. */
    [[nodiscard]] session::TaggedOps::Result fetchAxisProfile(ProfileSlot& slot) noexcept;
    /** Смена фазы; Ready / *Failed → Console::onAxisProfile. */
    void setFetchState(FetchState state, uint8_t axis_id = 0) noexcept;
    /** Первый слот с !profile.isValid(); нет — nullptr. */
    [[nodiscard]] ProfileSlot* nextPendingSlot() noexcept;
    /** Сбой ответа Get (не busy begin); при лимите — FetchFailed. */
    void noteFetchFail(uint8_t axis_id) noexcept;
    /** Bind profile этой сессии к оси (route.server_id == peerId). */
    void tryBindProfile(Axis& axis) noexcept;

    ProfileRange* _profiles = nullptr;
    /** Последний успешно применённый AxisRegistered (wire — _axis_registered). */
    segment::AxisRegisteredInfo _mapped{};
    /** Слот текущего Get AxisProfile (один in-flight); иначе nullptr. */
    ProfileSlot* _fetch_slot = nullptr;
    FetchState _fetch_state = FetchState::Idle;
    /** Подряд сбоев onReply по текущему слоту; сброс на успех / drop. */
    uint8_t _fetch_fails = 0;
};

/**
 * segment::Console + пул диапазонов Profile (leaf: SlotStore + RangeAllocator).
 * Сессии — show::Session. SessionBank / Profile / axes — у leaf.
 */
class Console : public segment::Console {
public:
    /** Рабочий Patch (APCH/ATYP); nullptr = нет шоу. Задать в ctor / leaf после Show. */
    const IPatch* patch;

    [[nodiscard]] Session* start(uint8_t server_id) noexcept
    {
        return static_cast<Session*>(segment::Console::start(server_id));
    }
    [[nodiscard]] Session* session(uint8_t server_id) noexcept
    {
        return static_cast<Session*>(segment::Console::session(server_id));
    }
    [[nodiscard]] Axis* axis(segment::console::idx_t slot) noexcept
    {
        return static_cast<Axis*>(segment::Console::axis(slot));
    }

    /**
     * Проход APCH: reset → bindPatch / start → Unpatched|NoSession|Patched.
     * onAxisPatch — когда все назначенные оси в терминальном PatchState
     * (после bindProfiles / сразу, если нечего ждать).
     */
    void remapAxes() noexcept;

    /** Сколько осей APCH назначено в этом remap (знаменатель прогресса). */
    [[nodiscard]] uint8_t patchAxisTotal() const noexcept { return _patch_axis_total; }
    /** Сколько из них уже в терминальном PatchState (числитель). */
    [[nodiscard]] uint8_t patchAxisReady() const noexcept { return _patch_axis_ready; }

protected:
    friend class Session;
    friend class Axis;

    explicit Console(ILink& link, ClockFn clock, const IPatch* patch = nullptr) noexcept
        : segment::Console(link, clock)
        , patch(patch)
    {}

    /**
     * Общий пул диапазонов Profile (leaf).
     * Session держит AllocRange* сессии (снаружи — profile()).
     */
    [[nodiscard]] virtual ProfileAllocator& profileAlloc() noexcept = 0;

    /**
     * FSM кэша AxisProfile. Ready → Session::bindProfiles (кроме догона remap).
     */
    virtual void onAxisProfile(Session* session, FetchState state,
                               uint8_t axis_id = 0) noexcept;

    /**
     * Все назначенные APCH-оси в терминальном PatchState
     * (patchAxisReady() == patchAxisTotal()).
     */
    virtual void onAxisPatch() noexcept {}

    /** Up + patch → requestRegistered; Down → releaseProfiles. Bind — onAxisProfile. */
    void onLink(::smcp::Session* session, Link link) noexcept override;

    /**
     * Ack Set AxisProfile → requestRegistered (сервер уже ++revision).
     */
    void onAck(::smcp::Session* session, const TxSlot& req,
               const session::Ack& reply) noexcept override;

    /** Смена PatchState + прогресс → возможно onAxisPatch. */
    void setAxisPatchState(Axis& axis, PatchState s) noexcept;

private:
    /** Снять patch+profile со всех осей inventory. */
    void resetStates() noexcept;

    uint8_t _patch_axis_total = 0;
    uint8_t _patch_axis_ready = 0;
};

} // namespace show
} // namespace smcp
