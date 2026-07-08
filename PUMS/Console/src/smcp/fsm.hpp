/**
 * @file fsm.hpp
 * @brief Конечный автомат штанкета SMCP v1.5 (блок 2.1).
 */

#pragma once

#include <cstdint>

namespace smcp {

/** Состояния жизненного цикла механизма. */
enum class MechState : uint8_t {
    Idle = 0x00,     /**< Покой, тормоз закрыт. */
    Armed = 0x01,    /**< Взведён, ожидание физ. кнопки СТАРТ. */
    Moving = 0x02,   /**< Активное движение, тормоз открыт. */
    Stopping = 0x03, /**< Плавное замедление. */
    Fault = 0x04,    /**< Авария, блокировка до RESET_FAULT. */
};

/** Тормоз закрыт в покое, взведении и аварии. */
[[nodiscard]] constexpr bool isBrakeEngaged(MechState state) noexcept
{
    return state == MechState::Idle
        || state == MechState::Armed
        || state == MechState::Fault;
}

/** Движение или замедление — тормоз может быть открыт. */
[[nodiscard]] constexpr bool isInMotion(MechState state) noexcept
{
    return state == MechState::Moving || state == MechState::Stopping;
}

/** Рабочий концевик вызывает STOPPING только в MOVING. */
[[nodiscard]] constexpr bool workLimitTriggersStopping(MechState state) noexcept
{
    return state == MechState::Moving;
}

/** Аварийные датчики немедленно переводят в FAULT. */
[[nodiscard]] constexpr bool sensorTriggersFault(MechState /*state*/) noexcept
{
    return true;
}

} // namespace smcp
