/**
 * @file axis.hpp
 * @brief Ось SMCP: Profile / Route + IAxis (runtime).
 *
 * smcp::axis — POD железа (Profile) и адрес (Route).
 * Шоу (ShowType / Patch) — smcp/show/show_axis.hpp.
 * smcp::IAxis — live + holder + phys axis_id + TX.
 *
 * База единиц (как в SI для длины; угол — градус, не радиан):
 *   Linear → метр, Rotary → градус.
 * Шкала — UnitScale (E0…E4 + Count → MISC::EnumValue); значение = степень p,
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

/** axis_id на одном server_id: 0…kAxisIdMax. */
inline constexpr uint8_t kAxisIdMax = 31u;

/** Адрес одной оси на шине: сегмент + axis_id. */
struct Route {
    uint8_t server_id = kServerIdNone;
    uint8_t axis_id = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return server_id == kServerIdNone; }

    /** Пустой route ок; иначе axis_id в 0…kAxisIdMax. */
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return empty() || axis_id <= kAxisIdMax;
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
 * Плотный 0…Count-1 → MISC::EnumValue; isValid() через обёртку.
 */
enum class Kind : uint8_t {
    Linear = 0, /**< База СИ — метр. */
    Rotary = 1, /**< База — градус (не радиан; удобнее для сцены). */
    Count,
};

/**
 * Шкала хранения относительно базы (м / °). Значение = степень p; base = value / 10^p.
 * Плотный 0…Count-1 → MISC::EnumValue. UI: E0 → м/°, E1 → дм, E2 → см, E3 → мм, E4 → 0.1 мм.
 */
enum class UnitScale : uint8_t {
    E0 = 0, /**< / 10^0. */
    E1,     /**< / 10^1. */
    E2,     /**< / 10^2. */
    E3,     /**< / 10^3 (дефолт). */
    E4,     /**< / 10^4. */
    Count,
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
 * Profile одной физической оси сегмента (axis_id 0…31).
 * scale / home энкодера / raw load — внутри привода, сюда не входят.
 * Invert шоу — в show::Patch, не здесь.
 */
struct alignas(uint32_t) Profile {
    uint8_t axis_id = 0; /**< axis_id на сервере. */
    MISC::EnumValue<Kind> kind{Kind::Linear};
    MISC::EnumValue<UnitScale> scale{UnitScale::E3}; /**< Шкала сервера. */
    REG::BitMask<Flag> flag{};

    /** Жёсткие пределы железа (ед. сервера). */
    struct {
        Range<int32_t> pos{};
        Range<uint16_t> speed{};      /**< min = vmin (0 — нет пола), max = vmax. */
        RangeMax<uint16_t> accel{}; /**< Разгон (ед./s²). */
        RangeMax<uint16_t> decel{}; /**< Торможение (ед./s²). */
    } limits{};

    uint8_t reserved_0[4]{};

    char name[kNameSize]{}; /**< Имя железа («Штанкет 12»). */

    void setName(const char* axis_name) noexcept { detail::copyName(name, axis_name); }

    void clear() noexcept
    {
        const uint8_t keep_id = axis_id;
        *this = Profile{};
        axis_id = keep_id;
    }
};

static_assert(sizeof(Profile) == 40u, "axis::Profile wire size");
static_assert(alignof(Profile) == alignof(uint32_t));
static_assert(std::is_standard_layout_v<Profile>);

} // namespace axis

/** Ось не выделена ни одной консолью. */
inline constexpr uint8_t kHolderNone = 0u;

/**
 * Тип хода SetTarget / слота пресета.
 * Единицы — те, что объявил kind оси, не миллиметры.
 */
enum class MoveMode : uint8_t {
    Absolute = 0, /**< Цель — абсолютная позиция. */
    Relative = 1, /**< Цель — дельта от текущей позиции. */
    Count,
};

[[nodiscard]] inline const char* cstr(MoveMode mode) noexcept
{
    switch (mode) {
    case MoveMode::Absolute: return "Absolute";
    case MoveMode::Relative: return "Relative";
    case MoveMode::Count: break;
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

/**
 * Live + Select/TX + phys axis_id (0…31).
 * Inventory ObjRegistry / Selection — здесь. CMech, DriveMech, show::AxisProxy.
 */
class IAxis {
public:
    /**
     * Маска состояния.
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

    explicit IAxis(uint8_t id) noexcept;
    virtual ~IAxis();

    /** ID оси в сегменте server ID (axis_id, 0…31). */
    [[nodiscard]] uint8_t id() const noexcept;

    /** Текущая позиция в единицах kind оси. */
    [[nodiscard]] int32_t position() const noexcept;

    /** Маска состояния (ready, moving, limit, fault и т.д.). */
    [[nodiscard]] REG::BitMask<Status> status() const noexcept;

    [[nodiscard]] bool isIdle() const noexcept;

    /**
     * ID консоли, держащей Select.
     * @return 0 — ось свободна.
     */
    [[nodiscard]] uint8_t holder() const noexcept;

    [[nodiscard]] bool isSelected() const noexcept;

    [[nodiscard]] bool isSelectedBy(uint8_t console_id) const noexcept;

    [[nodiscard]] bool isBlocked() const noexcept;

    /**
     * Заблокировать ось (Blocked) или снять блокировку.
     * Отказ — no-op (проверки снаружи / политика сервера).
     */
    virtual void block(bool blocked) noexcept = 0;

    /**
     * Выделить ось консолью (Select) или снять выделение (Deselect).
     * @param console_id ID консоли (SRC_ID); 0 — сброс выделения.
     * Политика отказа — до вызова (`acceptSelect` / mechGuard), здесь commit.
     */
    virtual void select(uint8_t console_id) noexcept = 0;

    /** Задать целевую позицию. Отказ — no-op (проверки снаружи / политика сервера). */
    virtual void setTarget(const MotionTarget& target) noexcept = 0;

    /** Сбросить локальный бит Fault. Отказ — no-op. */
    virtual void resetFault() noexcept = 0;

protected:
    void setId(uint8_t id) noexcept { _id = id; }

    uint8_t _id = 0;
    uint8_t _holder = kHolderNone;
    int32_t _position = 0;
    REG::BitMask<Status> _status{};

private:
    template <typename, typename>
    friend class MISC::ObjRegistry;

    /** Только из ObjRegistry при registerAt / registerAuto. */
    void set_id(uint8_t id) noexcept { setId(id); }
};

REG_BITMASK_ENUM_OPS(IAxis::Status)

} // namespace smcp
