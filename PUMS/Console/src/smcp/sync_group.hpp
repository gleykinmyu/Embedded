/**
 * @file sync_group.hpp
 * @brief Поведение синхронных групп штанкетов SMCP v1.5 (блок 2.4).
 */

#pragma once

#include <cstdint>

#include "constants.hpp"
#include "fsm.hpp"
#include "group.hpp"

namespace smcp {

/** FAULT любого участника останавливает всю группу («сцепка»). */
[[nodiscard]] inline bool syncFaultPropagates(MechState member_state) noexcept
{
    return member_state == MechState::Fault;
}

/** Рассогласование позиций участников превышает Sync_Tolerance. */
[[nodiscard]] inline bool isSyncDesync(int32_t delta_mm) noexcept
{
    const auto abs_delta = (delta_mm < 0) ? -delta_mm : delta_mm;
    return abs_delta > static_cast<int32_t>(Dynamics::sync_tolerance_mm);
}

/** Отпускание кнопки СТАРТ переводит всех участников в STOPPING. */
[[nodiscard]] inline bool syncStopOnStartRelease(bool start_pressed) noexcept
{
    return !start_pressed;
}

} // namespace smcp
