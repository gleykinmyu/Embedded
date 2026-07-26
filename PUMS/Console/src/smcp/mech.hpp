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

/** Идентификатор механизма в сегменте одного server ID (0..31). */
inline constexpr uint8_t kMechIdMax = 31u;

/** Механизм не выделен ни одной консолью. */
inline constexpr uint8_t kSelectOwnerNone = 0u;

/** Параметры взведённого движения (SetTarget). */
struct MotionTarget {
    int32_t target_mm = 0;
    uint16_t speed_mm_s = 0;
    uint16_t accel_mm_s2 = 0;
};

static_assert(sizeof(MotionTarget) == 8u);
static_assert(alignof(MotionTarget) == alignof(int32_t));

/**
 * Абстракция одного механизма: телеметрия и управление.
 * Реализация — на стороне консоли (model/Mech) или сегмента (DriveMech).
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
        Blocked  = 1u << 2, /**< Заблокирован (ручная / blocked-группа шоу). */
        Moving   = 1u << 3, /**< Выполняется подвод к цели. */
        Limit1   = 1u << 5, /**< Активен концевик / датчик границы 1. */
        Limit2   = 1u << 6, /**< Активен концевик / датчик границы 2. */
        Fault    = 1u << 7, /**< Авария: движение запрещено до сброса. */
    };

    explicit IMech(uint8_t id) noexcept;

    virtual ~IMech();

    /** ID механизма в сегменте server ID (mech_id, 0..31). */
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

    [[nodiscard]] bool isBlocked() const noexcept;

    /**
     * Заблокировать механизм (Blocked) или снять блокировку.
     * @return false — операция отклонена реализацией.
     */
    virtual bool block(bool blocked) noexcept = 0;

    /**
     * Выделить механизм консолью (Select) или снять выделение (Deselect).
     * @param console_id ID консоли (SRC_ID); 0 — сброс выделения.
     */
    virtual bool select(uint8_t console_id) noexcept = 0;

    /** Задать целевую позицию, мм. */
    virtual bool setTarget(const MotionTarget& target) noexcept = 0;

    /** Сбросить локальный бит Fault. */
    virtual bool resetFault() noexcept = 0;

    /** Обновить состояние из телеметрии (@a src_id — SRC_ID кадра, обычно сервер). */
    virtual void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

protected:
    uint8_t _id;
    uint8_t _select_owner_id = kSelectOwnerNone;
    int32_t _position = 0;
    REG::BitMask<Status> _status{};
};

REG_BITMASK_ENUM_OPS(IMech::Status)

} // namespace smcp
