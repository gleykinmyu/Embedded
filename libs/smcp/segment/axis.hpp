/**
 * @file axis.hpp
 * @brief Ось SMCP: Profile / Route + IAxis (runtime).
 *
 * smcp::axis — POD железа (Profile) и адрес (Route).
 * Шоу (show::AxisType / PatchRec / Patch) + show::Axis — show/patch.hpp, show/console.hpp.
 * smcp::IAxis — pure interface; состояние у segment::server::Axis / segment::console::Axis.
 *
 * База единиц (как в SI для длины; угол — градус, не радиан):
 *   Linear → метр, Rotary → градус.
 * Шкала — UnitScale (E0…E4, First/Last → MISC::EnumValue); значение = степень p,
 *   base = value / 10^p. Пример: value=1500, E3 → 1.5 м / 1.5°.
 * Скорость / ускорение — та же шкала /s, /s².
 * На сервер: value_srv = value_console × 10^(p_server − p_console).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bitmask.hpp"
#include "enum_value.hpp"
#include "obj_registry.hpp"

namespace smcp {
namespace axis {

inline constexpr std::size_t kNameSize = 16u;
inline constexpr uint8_t kServerIdNone = 0u;

/** Число осей на одном server_id; axis_id ∈ [0, kAxisCount). */
inline constexpr uint8_t kAxisCount = 32u;
/** Последний допустимый axis_id (= kAxisCount - 1). */
inline constexpr uint8_t kAxisIdMax = static_cast<uint8_t>(kAxisCount - 1u);

/** Адрес одной оси на шине: сегмент + axis_id. */
struct Route {
    uint8_t server_id = kServerIdNone;
    uint8_t axis_id = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return server_id == kServerIdNone; }

    /** Пустой: axis_id == 0; иначе axis_id в 0…kAxisIdMax. */
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return empty() ? (axis_id == 0u) : (axis_id <= kAxisIdMax);
    }
};

static_assert(sizeof(Route) == 2u);
static_assert(std::is_standard_layout_v<Route>);

namespace detail {

inline void copyName(char (&dst)[kNameSize], const char* src) noexcept
{
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    std::strncpy(dst, src, kNameSize - 1u);
    dst[kNameSize - 1u] = '\0';
}

} // namespace detail

/**
 * Тип оси. FlightObject — не kind, а объект над осями (отдельно).
 * First…Last → MISC::EnumValue; isValid() через обёртку.
 */
enum class Kind : uint8_t {
    First = 0,
    Linear = First, /**< База СИ — метр. */
    Rotary,         /**< База — градус (не радиан; удобнее для сцены). */
    Last,           /**< Exclusive end. */
};

/**
 * Шкала хранения относительно базы (м / °). Значение = степень p; base = value / 10^p.
 * [First, Last) → MISC::EnumValue. UI: E0 → м/°, … E4 → 0.1 мм.
 */
enum class UnitScale : uint8_t {
    First = 0,
    E0 = First, /**< / 10^0. */
    E1,         /**< / 10^1. */
    E2,         /**< / 10^2. */
    E3,         /**< / 10^3 (дефолт). */
    E4,         /**< / 10^4. */
    Last,       /**< Exclusive end. */
};

[[nodiscard]] constexpr uint8_t scalePow(UnitScale s) noexcept
{
    return static_cast<uint8_t>(s);
}

/** Свойства привода / модели на сервере. */
enum class Flag : uint8_t {
    Continuous = 1u << 0, /**< Без упора: оборот / кольцевая дорога; hard на краях int. */
    FixedSpeed = 1u << 1, /**< Только on/off; рабочая скорость = vmax. */
    LoadSense = 1u << 2,  /**< Есть датчик нагрузки (окно — у привода). */
    All = Continuous | FixedSpeed | LoadSense,
};

REG_BITMASK_ENUM_OPS(Flag)

/** Диапазон {min, max}. */
template <typename T>
struct Range {
    T min{};
    T max{};
};

/** Только потолок (шоу без vmin). Доступ: .max — как у Range. */
template <typename T>
struct RangeMax {
    T max{};
};

static_assert(sizeof(Range<int32_t>) == 8u);
static_assert(sizeof(Range<uint16_t>) == 4u);
static_assert(sizeof(RangeMax<uint16_t>) == 2u);
static_assert(std::is_standard_layout_v<Range<int32_t>>);
static_assert(std::is_standard_layout_v<RangeMax<uint16_t>>);

/**
 * Profile одной физической оси сегмента.
 * scale / home энкодера / raw load — внутри привода, сюда не входят.
 * Invert шоу — в show::PatchRec, не здесь.
 */
struct alignas(uint32_t) Profile {
    MISC::EnumValue<Kind> kind{Kind::Linear};
    MISC::EnumValue<UnitScale> scale{UnitScale::E3}; /**< Шкала сервера. */
    REG::BitMask<Flag> flag{};
    uint8_t axis_id = 0xFFu; /**< Phys id в сегменте; 0xFF — пусто / ещё не задан. */

    /** Жёсткие пределы железа (ед. сервера). */
    struct {
        Range<int32_t> pos{};
        Range<uint16_t> speed{};      /**< min = vmin (0 — нет пола), max = vmax. */
        RangeMax<uint16_t> accel{}; /**< Разгон (ед./s²). */
        RangeMax<uint16_t> decel{}; /**< Торможение (ед./s²). */
    } limits{};

    uint8_t reserved_1[4]{};

    char name[kNameSize]{}; /**< Имя железа («Штанкет 12»). */

    void setName(const char* axis_name) noexcept { detail::copyName(name, axis_name); }

    void clear() noexcept { *this = Profile{}; }

    /**
     * Пометить «нужен Get»: clear + @a id + kind/scale = Last → !isValid().
     * clear() одних дефолтов (Linear/E3) для pending мало — они валидны.
     */
    void markDirty(uint8_t id) noexcept
    {
        clear();
        axis_id = id;
        kind = Kind::Last;
        scale = UnitScale::Last;
    }

    /**
     * Паспорт пригоден: kind + scale в диапазоне.
     * До Get консоль держит Last (!isValid); clear() — валидные дефолты.
     */
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return kind.isValid() && scale.isValid();
    }
};

static_assert(sizeof(Profile) == 40u, "axis::Profile wire size");
static_assert(alignof(Profile) == alignof(uint32_t));
static_assert(std::is_standard_layout_v<Profile>);

/**
 * Маска live-состояния оси.
 * Ready/Moving — фаза движения; остальные биты — параллельные условия.
 * Idle — ни Ready, ни Moving не установлены.
 */
enum class Status : uint8_t {
    Ready   = 1u << 0, /**< Привод готов к пуску. */
    Blocked = 1u << 2, /**< Сегментный запрет (сервер); зеркало на пульте из Telemetry. */
    Moving  = 1u << 3, /**< Выполняется подвод к цели. */
    Limit1  = 1u << 5, /**< Активен концевик / датчик границы 1. */
    Limit2  = 1u << 6, /**< Активен концевик / датчик границы 2. */
    Fault   = 1u << 7, /**< Авария: движение запрещено до сброса. */
    All = Ready | Blocked | Moving | Limit1 | Limit2 | Fault,
};

REG_BITMASK_ENUM_OPS(Status)

/**
 * Тип хода SetTarget / слота пресета.
 * Единицы — те, что объявил kind оси, не миллиметры.
 */
enum class MoveMode : uint8_t {
    First = 0,
    Absolute = First, /**< Цель — абсолютная позиция. */
    Relative,         /**< Цель — дельта от текущей позиции. */
    Last,             /**< Exclusive end. */
};

[[nodiscard]] inline const char* cstr(MoveMode mode) noexcept
{
    switch (mode) {
    case MoveMode::Absolute: return "Absolute";
    case MoveMode::Relative: return "Relative";
    case MoveMode::Last: break;
    }
    return "?";
}

/**
 * Параметры взведённого движения (SetTarget).
 * target/speed — signed / unsigned в единицах kind. Ускорение на шину не кладётся.
 */
struct MotionTarget {
    int32_t target = 0;
    uint16_t speed = 0;
    MoveMode mode = MoveMode::Absolute;
    uint8_t _reserved = 0;
};

static_assert(sizeof(MotionTarget) == 8u);
static_assert(alignof(MotionTarget) == alignof(int32_t));

} // namespace axis

/** Ось не выделена ни одной консолью. */
inline constexpr uint8_t kHolderNone = 0u;


/**
 * Чистый интерфейс оси: live-снимок + Select/Block/SetTarget/ResetFault.
 * Состояние и id — у segment::*::Axis / show::AxisProxy.
 */
class IAxis {
public:
    virtual ~IAxis() = default;

    /** Текущая позиция в единицах kind оси. */
    [[nodiscard]] virtual int32_t position() const noexcept = 0;

    /** Маска состояния (ready, moving, limit, fault и т.д.). */
    [[nodiscard]] virtual REG::BitMask<axis::Status> status() const noexcept = 0;

    /**
     * ID консоли, держащей Select.
     * @return 0 — ось свободна.
     */
    [[nodiscard]] virtual uint8_t holder() const noexcept = 0;

    [[nodiscard]] bool isIdle() const noexcept
    {
        return !status().any(axis::Status::Ready | axis::Status::Moving);
    }

    [[nodiscard]] bool isSelected() const noexcept { return holder() != kHolderNone; }

    [[nodiscard]] bool isSelectedBy(uint8_t console_id) const noexcept
    {
        return console_id != kHolderNone && holder() == console_id;
    }

    [[nodiscard]] bool isBlocked() const noexcept
    {
        return status().any(axis::Status::Blocked);
    }

    /**
     * Заблокировать ось (Blocked) или снять блокировку.
     * @return false — отказ (нет route/сессии; политика — до вызова на сервере).
     */
    [[nodiscard]] virtual bool block(bool blocked) noexcept = 0;

    /**
     * Выделить ось консолью (Select) или снять выделение (Deselect).
     * @param console_id ID консоли (SRC_ID); 0 — сброс выделения.
     * Политика отказа на сервере — до вызова (`acceptSelect` / mechGuard), здесь commit.
     * @return false — отказ (нет route/сессии / TX не встал).
     */
    [[nodiscard]] virtual bool select(uint8_t console_id) noexcept = 0;

    /**
     * Задать целевую позицию.
     * @return false — отказ (нет route/сессии / TX не встал).
     */
    [[nodiscard]] virtual bool setTarget(const axis::MotionTarget& target) noexcept = 0;

    /**
     * Сбросить локальный бит Fault.
     * @return false — отказ (нет route/сессии / TX не встал).
     */
    [[nodiscard]] virtual bool resetFault() noexcept = 0;
};

} // namespace smcp
