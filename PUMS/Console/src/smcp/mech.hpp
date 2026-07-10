/**
 * @file mech.hpp
 * @brief Интерфейс механизма (штанкета) SMCP.
 */

#pragma once

#include <cstdint>

#include "bitmask.hpp"

namespace smcp {

/** Локальный идентификатор механизма в сегменте (0..63). */
inline constexpr uint8_t kMechIdMax = 63u;

/** Механизм свободен (lease не захвачен). */
inline constexpr uint8_t kLeaseOwnerNone = 0u;

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
        Ready   = 1u << 0, /**< Привод готов к пуску. */
        Moving  = 1u << 1, /**< Выполняется подвод к цели. */
        Inhibit = 1u << 2, /**< Внешний запрет движения (E-stop, блокировка сцены). */
        Limit1  = 1u << 5, /**< Активен концевик / датчик границы 1. */
        Limit2  = 1u << 6, /**< Активен концевик / датчик границы 2. */
        Fault   = 1u << 7, /**< Авария: движение запрещено до сброса. */
    };

    explicit IMech(uint8_t id) noexcept;

    virtual ~IMech();

    /** Локальный ID механизма в сегменте. */
    [[nodiscard]] uint8_t id() const noexcept;

    /** Текущая позиция, мм. */
    [[nodiscard]] virtual int32_t position() const noexcept = 0;

    /** Маска состояния (ready, moving, limit, fault и т.д.). */
    [[nodiscard]] virtual REG::BitMask<Status> status() const noexcept = 0;

    [[nodiscard]] bool isIdle() const noexcept;

    /**
     * ID консоли, захватившей lease.
     * @return 0 — механизм свободен.
     */
    [[nodiscard]] uint8_t leaseOwner() const noexcept;

    [[nodiscard]] bool isLeased() const noexcept;

    [[nodiscard]] bool isLeasedBy(uint8_t console_id) const noexcept;

    /** Задать целевую позицию, мм. */
    virtual bool setTarget(int32_t position_mm) noexcept = 0;

    /** Остановить движение. */
    virtual bool stop() noexcept = 0;

protected:
    void setLeaseOwner(uint8_t console_id) noexcept;

    uint8_t id_;
    uint8_t lease_owner_ = kLeaseOwnerNone;
};

REG_BITMASK_ENUM_OPS(IMech::Status)

} // namespace smcp
