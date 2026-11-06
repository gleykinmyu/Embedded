/**
 * @file message.hpp
 * @brief Northbound PDU (консоль ↔ сервер сегмента). Не транспорт.
 *
 * Demux: server::Session (unicast A); Telemetry D → console::Session::onPacket (src).
 * Get/Set/DataBegin/Data — транспорт `smcp/message.hpp` (Node / Session).
 */

#pragma once

#include <cstdint>

#include "enum_value.hpp"
#include "segment/axis.hpp"
#include "segment/selection.hpp"
#include "smcp/message.hpp"

namespace smcp::segment {

/**
 * Northbound MsgId (оси / сервис PIN).
 * Get/Set/DataBegin/Data — `smcp::session::msgId` в libs/smcp. 0x30 — Heartbeat.
 */
enum class msgId : uint8_t {
    /** Пульт берёт или отдаёт оси: маска + Add / Remove / Set. */
    Select = 0x10,
    /** Сегментный запрет движения: маска + Add / Remove / Set. Не флаг группы на пульте. */
    Block = 0x11,
    /** Уставка: одна ось или всё Select консоли (флаг all_selected → DLC 7/8). */
    SetTarget = 0x20,
    /** Снимок осей по маске. Состояние сегмента не меняет. Пустая маска — весь inventory. */
    GetTelemetry = 0x21,
    /** Пустая маска — globalFault (GPIO сервера). Биты — Fault указанных осей. */
    ResetFault = 0x22,
    /**
     * Сервисный доступ на сессию: DLC=0 → lock; DLC=4 pin_hash → unlock.
     * Без lock по истечении ServiceAccess::kHoldMs — авто-lock.
     */
    ServiceAccess = 0x23,
    /** Снимок оси: кто держит, статус, позиция в единицах kind. */
    Telemetry = 0x40,
};

/** Адреса northbound. Broadcast 0xFF — транспорт (`Packet::kBroadcastId`). */
inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
/** Сколько консолей / сессий на сервере (id 0x01…0x0F). */
inline constexpr uint8_t kMaxConsoles = kConsoleIdMax - kConsoleIdMin + 1u;
inline constexpr uint8_t kServerIdMin = 0x10u;
inline constexpr uint8_t kServerIdMax = 0xEFu;

/**
 * Прикладные Nack northbound (слой после @c smcp::session::vError::Last).
 * Ok → @c smcp::session::Error::None; локальный Timeout → @c smcp::session::Error::Timeout.
 */
enum class Error : uint8_t {
    First = session::vError::Last, /**< 0x10. */
    Busy = First, /**< Ось / ServiceAccess у другой консоли. */
    Limits,       /**< Концевики / пределы хода (SetTarget). */
    Crc,          /**< Ошибка CRC / целостности. */
    AxisNotFound, /**< Нет оси / id > kAxisIdMax. */
    Safety,       /**< Blocked / нет ServiceAccess. */
    NotReady,     /**< Привод не Ready. */
    SelectLimit,  /**< Политика сегмента (лимит Select). */
    Last,         /**< Exclusive end (авто после SelectLimit). */
};

/** smcp::session::vError + Error; Ok = None. */
class vError : public MISC::EnumValueChain<Error, session::vError> {
    using Base = MISC::EnumValueChain<Error, session::vError>;

public:
    constexpr vError() noexcept = default;
    constexpr vError(Error e) noexcept : Base(e) {}
    constexpr vError(session::Error e) noexcept : Base(e) {}
    constexpr vError(const session::vError& p) noexcept : Base(p) {}

    [[nodiscard]] static constexpr vError from_raw(word_type w) noexcept
    {
        vError v{};
        v.set_raw(w);
        return v;
    }

    [[nodiscard]] const char* cstr() const noexcept
    {
        if (isOwn()) {
            switch (static_cast<Error>(*this)) {
            case Error::Busy: return "Busy";
            case Error::Limits: return SMCP_PICK("Lim", "Limits");
            case Error::Crc: return "Crc";
            case Error::AxisNotFound: return SMCP_PICK("Axis", "AxisNotFound");
            case Error::Safety: return SMCP_PICK("Safe", "Safety");
            case Error::NotReady: return SMCP_PICK("NRdy", "NotReady");
            case Error::SelectLimit: return SMCP_PICK("SLim", "SelectLimit");
            case Error::Last: break;
            }
            return "?";
        }
        return session::vError::cstr();
    }
};

/**
 * Теги Get/Set (TaggedOps). index = axis_id или @c kIndexAll.
 * struct + operator uint8_t(): сравнение / send без cast (у enum class так нельзя).
 */
struct ObjTag {
    uint8_t value = 0;

    /** Весь объект tag (не один axis_id). Тот же wire, что `smcp::session::kIndexAll`. */
    static constexpr uint8_t kIndexAll = session::kIndexAll;

    constexpr ObjTag() = default;
    constexpr explicit ObjTag(uint8_t v) noexcept : value(v) {}

    constexpr operator uint8_t() const noexcept { return value; }

    /**
     * axis::Profile; Get/Set по одному axis_id (+ PIN на Set).
     * @c kIndexAll — Nack NotSupported (консоль тянет по одной после AxisRegistered).
     */
    static const ObjTag AxisProfile;

    /**
     * Get-only: @ref AxisRegisteredInfo (selection u32 LE + revision u16 LE = 6 байт,
     * один inline Data). Консоль дальше тянет AxisProfile по битам маски;
     * revision++ на сервере при Set AxisProfile — сигнал перечитать паспорта.
     */
    static const ObjTag AxisRegistered;
};

inline constexpr ObjTag ObjTag::AxisProfile{0x01};
inline constexpr ObjTag ObjTag::AxisRegistered{0x02};

/**
 * Тело Get AxisRegistered: inventory + счётчик ревизии паспортов.
 * Ровно 6 байт на wire (без хвоста padding в IBuf.size).
 *
 * @c revision — поколение паспортов: ++ при Set AxisProfile (wrap ok).
 */
#pragma pack(push, 1)
struct AxisRegisteredInfo {
    Selection selection{};
    uint16_t revision = 0;
};
#pragma pack(pop)

static_assert(sizeof(Selection) == 4u);
static_assert(sizeof(AxisRegisteredInfo) == 6u);

[[nodiscard]] constexpr bool operator==(const AxisRegisteredInfo& a,
                                        const AxisRegisteredInfo& b) noexcept
{
    return static_cast<uint32_t>(a.selection) == static_cast<uint32_t>(b.selection)
        && a.revision == b.revision;
}
[[nodiscard]] constexpr bool operator!=(const AxisRegisteredInfo& a,
                                        const AxisRegisteredInfo& b) noexcept
{
    return !(a == b);
}

[[nodiscard]] inline const char* cstr(Error code) noexcept
{
    return vError{code}.cstr();
}

[[nodiscard]] inline const char* cstr(vError code) noexcept
{
    return code.cstr();
}

/**
 * Общий action для Select / Block (маска осей).
 * Add/Remove — дельта по битам; Set — абсолютная маска.
 */
enum class Action : uint8_t {
    First = 0,
    Add = First, /**< Select / Block. */
    Remove,      /**< Deselect / Unblock. */
    Set,         /**< Absolute mask. */
    Last,        /**< Exclusive end. */
};

[[nodiscard]] inline const char* cstr(Action action) noexcept
{
    switch (action) {
    case Action::Add: return "Add";
    case Action::Remove: return "Remove";
    case Action::Set: return "Set";
    case Action::Last: break;
    }
    return "?";
}

namespace detail {

[[nodiscard]] inline bool unpackMaskOp(const Message& m, Action& action,
                                       Selection& selection) noexcept
{
    if (m.dlc != 5) {
        return false;
    }
    const auto a = MISC::EnumValue<Action>::from_raw(m.data[0]);
    if (!a.isValid()) {
        return false;
    }
    action = a;
    selection = Selection::from_raw(m.loadU32(1));
    return true;
}

[[nodiscard]] inline bool packMaskOp(Message& m, Action action, Selection selection) noexcept
{
    return m.push(static_cast<uint8_t>(action)) && m.pushU32(selection.raw());
}

} // namespace detail

/**
 * Select — DLC=5
 *   +--------+-------------+
 *   | action | mask LE u32 |
 *   +--------+-------------+
 *    data[0]  data[1..4]     (txn_id → CAN ID)
 */
struct Select {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::Select);
    SMCP_NAME("Sel", "Select")

    Action action = Action::Add;
    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return detail::packMaskOp(m, action, selection);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        return detail::unpackMaskOp(m, action, selection);
    }
};

/**
 * Block — DLC=5 (как Select)
 */
struct Block {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::Block);
    SMCP_NAME("Blk", "Block")

    Action action = Action::Add;
    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return detail::packMaskOp(m, action, selection);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        return detail::unpackMaskOp(m, action, selection);
    }
};

/**
 * GetTelemetry — DLC=4 (класс A)
 *   +-------------+
 *   | mask LE u32 |
 *   +-------------+
 *    data[0..3]
 *
 * Пустая маска = все оси inventory сервера.
 * Ответ: Ack, затем Telemetry (D).
 */
struct GetTelemetry {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::GetTelemetry);
    SMCP_NAME("GT", "GetTelemetry")

    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.pushU32(selection.raw());
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 4) {
            return false;
        }
        selection = Selection::from_raw(m.loadU32(0));
        return true;
    }
};

/**
 * SetTarget — одна структура; флаг all_selected задаёт DLC.
 *
 * Общий префикс:
 *   +------+-----------+----------+
 *   | mode | target LE | speed LE |
 *   |  u8  |    i32    |   u16    |
 *   +------+-----------+----------+
 *    [0]    [1..4]       [5..6]
 *
 * DLC=7  — all_selected: уставка на все оси, которые эта консоль держит Select.
 * DLC=8  — одна ось: + axis_id в data[7].
 */
struct SetTarget {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::SetTarget);
    SMCP_NAME("Tg", "SetTarget")

    axis::MotionTarget target{};
    uint8_t axis_id = 0;
    bool all_selected = false; /**< true → без axis_id; сервер берёт Select этой консоли. */

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        if (!m.push(static_cast<uint8_t>(target.mode))
            || !m.pushU32(static_cast<uint32_t>(target.target))
            || !m.pushU16(target.speed)) {
            return false;
        }
        if (all_selected) {
            return true;
        }
        return m.push(axis_id);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 7 && m.dlc != 8) {
            return false;
        }
        const auto mode = MISC::EnumValue<axis::MoveMode>::from_raw(m.data[0]);
        if (!mode.isValid()) {
            return false;
        }
        target.mode = mode;
        target.target = static_cast<int32_t>(m.loadU32(1));
        target.speed = m.loadU16(5);
        target._reserved = 0;
        all_selected = (m.dlc == 7);
        axis_id = all_selected ? 0 : m.data[7];
        return true;
    }
};

/**
 * ResetFault — DLC=4 (класс A).
 *   +-------------+
 *   | mask LE u32 |
 *   +-------------+
 *    data[0..3]
 *
 * Пустая маска — globalFault, отдельные GPIO сервера (E-Stop release).
 * Ненулевая — сброс Fault только у указанных осей.
 */
struct ResetFault {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::ResetFault);
    SMCP_NAME("RF", "ResetFault")

    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.pushU32(selection.raw());
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 4) {
            return false;
        }
        selection = Selection::from_raw(m.loadU32(0));
        return true;
    }
};

/**
 * Telemetry — DLC=8 (broadcast, txn_id=0)
 *   +---------+----------+--------+--------+--------------+
 *   | axis_id | reserved | holder | status | position LE  |
 *   |   u8    |    u8    |  u8    |   u8   | i32 (kind)   |
 *   +---------+----------+--------+--------+--------------+
 */
struct Telemetry {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::Telemetry);
    SMCP_NAME("Te", "Telemetry")

    uint8_t axis_id = 0;
    uint8_t reserved = 0;
    uint8_t holder_id = kHolderNone;
    REG::BitMask<axis::Status> status{};
    int32_t position = 0;

    [[nodiscard]] static Telemetry from(uint8_t axis_id, const IAxis& a) noexcept
    {
        Telemetry t{};
        t.axis_id = axis_id;
        t.holder_id = a.holder();
        t.position = a.position();
        t.status = a.status();
        return t;
    }

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.push(axis_id)
            && m.push(reserved)
            && m.push(holder_id)
            && m.push(static_cast<uint8_t>(status.raw()))
            && m.pushU32(static_cast<uint32_t>(position));
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 8) {
            return false;
        }
        axis_id = m.data[0];
        reserved = m.data[1];
        holder_id = m.data[2];
        status = REG::BitMask<axis::Status>::from_raw(m.data[3]);
        position = static_cast<int32_t>(m.loadU32(4));
        return true;
    }
};

/**
 * ServiceAccess — класс A.
 *   DLC=0           → lock (снять сервис с сессии).
 *   DLC=4 pin_hash  → unlock (совпал с NVM сервера → Ack).
 * Держать unlock дольше @c kHoldMs без явного lock — авто-lock на сервере.
 */
struct ServiceAccess {
    static constexpr uint8_t kId = static_cast<uint8_t>(msgId::ServiceAccess);
    SMCP_NAME("SA", "ServiceAccess")
    static constexpr uint16_t kHoldMs = 60'000u;

    bool unlock = false;     /**< true — кадр с hash; false — lock. */
    uint32_t pin_hash = 0;

    [[nodiscard]] static ServiceAccess makeLock() noexcept
    {
        return ServiceAccess{};
    }
    [[nodiscard]] static ServiceAccess makeUnlock(uint32_t hash) noexcept
    {
        ServiceAccess b{};
        b.unlock = true;
        b.pin_hash = hash;
        return b;
    }

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        if (!unlock) {
            return true; /* DLC=0 */
        }
        return m.pushU32(pin_hash);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc == 0) {
            unlock = false;
            pin_hash = 0;
            return true;
        }
        if (m.dlc != 4) {
            return false;
        }
        unlock = true;
        pin_hash = m.loadU32(0);
        return true;
    }
};

/** Имя northbound PDU по байту id. Чужой id — транспортный каталог. */
[[nodiscard]] inline const char* cstr(uint8_t id) noexcept
{
    if (const char* n = ::smcp::cstr<Select, Block, GetTelemetry, SetTarget, ResetFault,
                                     ServiceAccess, Telemetry>(id)) {
        return n;
    }
    return session::cstr(id);
}

[[nodiscard]] constexpr uint8_t serverIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(kServerIdMin + (global_id / kMechCount));
}

[[nodiscard]] constexpr uint8_t mechIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(global_id % kMechCount);
}

[[nodiscard]] constexpr bool isConsoleId(uint8_t id) noexcept
{
    return id >= kConsoleIdMin && id <= kConsoleIdMax;
}

[[nodiscard]] constexpr bool isServerId(uint8_t id) noexcept
{
    return id >= kServerIdMin && id <= kServerIdMax;
}

} // namespace smcp::segment
