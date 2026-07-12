/**
 * @file mech.hpp
 * @brief Интерфейс механизма (штанкета) SMCP.
 */

#pragma once

#include <cstdint>

#include "bitmask.hpp"

namespace smcp {

namespace msg {
struct Telemetry;
}

/** Локальный идентификатор механизма в сегменте (0..63). */
inline constexpr uint8_t kMechIdMax = 63u;

/** Механизм не выделен ни одной консолью. */
inline constexpr uint8_t kSelectOwnerNone = 0u;

/** Параметры взведённого движения (SetTarget). */
#pragma pack(push, 1)
struct MotionTarget {
    int32_t target_mm = 0;
    uint16_t speed_mm_s = 0;
    uint16_t accel_mm_s2 = 0;
};
#pragma pack(pop)

static_assert(sizeof(MotionTarget) == 8u);

/**
 * Абстракция одного механизма: телеметрия и управление.
 * Реализация — на стороне драйвера сегмента / StageNet.
 */
class IMech {
public:
    /**
     * Маска состояния.
     * Ready/Moving — фаза движения; остальные биты — параллельные условия.
     * Idle — ни Ready, ни Moving не установлены.
     */
    enum class Status : uint8_t {
        Ready    = 1u << 0, /**< Привод готов к пуску. */
        Selected = 1u << 1, /**< Выделен консолью (select). */
        Inhibit  = 1u << 2, /**< Внешний запрет движения (E-stop, блокировка сцены). */
        Moving   = 1u << 3, /**< Выполняется подвод к цели. */
        Limit1   = 1u << 5, /**< Активен концевик / датчик границы 1. */
        Limit2   = 1u << 6, /**< Активен концевик / датчик границы 2. */
        Fault    = 1u << 7, /**< Авария: движение запрещено до сброса. */
    };

    explicit IMech(uint8_t id) noexcept;

    virtual ~IMech();

    /** Локальный ID механизма в сегменте. */
    [[nodiscard]] uint8_t id() const noexcept;

    /** Текущая позиция, мм. */
    [[nodiscard]] int32_t position() const noexcept;

    /** Маска состояния (ready, moving, limit, fault и т.д.). */
    [[nodiscard]] REG::BitMask<Status> status() const noexcept;

    [[nodiscard]] bool isIdle() const noexcept;

    /**
     * ID консоли, выделившей механизм.
     * @return 0 — механизм свободен.
     */
    [[nodiscard]] uint8_t select_owner_id() const noexcept;

    [[nodiscard]] bool isSelected() const noexcept;

    [[nodiscard]] bool isSelectedBy(uint8_t console_id) const noexcept;

    /**
     * Выделить механизм консолью (Select) или снять выделение (Deselect).
     * @param console_id ID консоли (SRC_ID); 0 — сброс выделения.
     */
    virtual bool select(uint8_t console_id) noexcept = 0;

    /** Задать целевую позицию, мм. */
    virtual bool setTarget(const MotionTarget& target) noexcept = 0;

    /** Сбросить локальный бит Fault (после ResetFault с сервера). */
    virtual bool resetFault() noexcept = 0;

    /** Обновить состояние из телеметрии (@a src_id — SRC_ID кадра, обычно сервер). */
    virtual void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

protected:
    uint8_t _id;
    uint8_t _select_owner_id = kSelectOwnerNone;
    int32_t _position = 0;
    REG::BitMask<Status> _status{};
};

/** Локальная реализация механизма консоли. */
class Mech : public IMech {
public:
    enum class Type : uint8_t {
        Rope,
        Chain,
    };

    Mech() noexcept;
    Mech(uint8_t id, Type type = Type::Rope) noexcept;

    [[nodiscard]] Type type() const noexcept;

    bool select(uint8_t console_id) noexcept override;
    bool setTarget(const MotionTarget& target) noexcept override;
    bool resetFault() noexcept override;

private:
    Type _type;
};

REG_BITMASK_ENUM_OPS(IMech::Status)

} // namespace smcp
