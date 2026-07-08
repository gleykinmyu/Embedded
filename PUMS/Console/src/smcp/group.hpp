/**
 * @file group.hpp
 * @brief Структуры данных групп штанкетов SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "constants.hpp"

namespace smcp {

/** FourCC-тег секции групп в шоуфайле — "GRUP". */
inline constexpr uint32_t kGroupSectionTag = 0x50555247u;

/** Флаги структуры Group (блок 3.3). */
enum GroupFlags : uint8_t {
    GroupLocked = 1u << 0,
    GroupSync = 1u << 1,
    GroupAtomic = 1u << 2,
    GroupBlocked = 1u << 3,
    GroupSetup = 1u << 4,
};

[[nodiscard]] constexpr bool hasGroupFlag(uint8_t flags, GroupFlags bit) noexcept
{
    return (flags & static_cast<uint8_t>(bit)) != 0u;
}

#pragma pack(push, 1)

/** Логическое объединение штанкетов — 64 байта. */
struct Group {
    uint16_t id = 0;
    uint64_t mech_mask = 0;
    char name[Layout::group_name]{};
    uint8_t flags = 0;
    uint16_t crc16 = 0;
    uint8_t reserved[3]{};
};

static_assert(sizeof(Group) == Layout::group);

/** Состояние активной синхронной группы в рантайме (блок 2.4). */
struct SyncGroupRuntime {
    uint16_t group_id = 0;
    uint64_t active_mask = 0;
    bool sync_start_armed = false;
    int32_t max_position_delta_mm = 0;
};

#pragma pack(pop)

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

} // namespace smcp
