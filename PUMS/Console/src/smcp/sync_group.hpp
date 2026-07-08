/**
 * @file sync_group.hpp
 * @brief Синхронные группы штанкетов SMCP v1.5 (блок 2.4).
 */

#pragma once

#include <cstdint>

#include "constants.hpp"
#include "fsm.hpp"
#include "status.hpp"
#include "types.hpp"

namespace smcp {

/** Состояние активной синхронной группы в рантайме. */
struct SyncGroupRuntime {
    uint16_t group_id = 0;
    uint64_t active_mask = 0;
    bool sync_start_armed = false;
    int32_t max_position_delta_mm = 0;
};

/**
 * Группа с флагом SYNC требует одновременного старта и остановки.
 * ATOMIC запрещает разделение маски при управлении.
 */
[[nodiscard]] inline bool isSyncGroup(const Group& group) noexcept
{
    return hasGroupFlag(group.flags, GroupFlags::GroupSync);
}

[[nodiscard]] inline bool isAtomicGroup(const Group& group) noexcept
{
    return hasGroupFlag(group.flags, GroupFlags::GroupAtomic);
}

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
