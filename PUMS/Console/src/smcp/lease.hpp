/**
 * @file lease.hpp
 * @brief Эксклюзивное владение штанкетами SMCP v1.5 (блок 2.5).
 */

#pragma once

#include <cstdint>

#include "constants.hpp"
#include "types.hpp"

namespace smcp {

/** Состояние lease-сессии для одного сегмента сервера. */
struct LeaseSession {
    uint64_t leased_mask = 0;
    uint8_t owner_id = 0;
    uint32_t last_heartbeat_ms = 0;

    [[nodiscard]] bool isLeased(uint8_t local_id) const noexcept
    {
        return (leased_mask & (1ull << local_id)) != 0ull;
    }

    [[nodiscard]] bool isOwnedBy(uint8_t console_id) const noexcept
    {
        return owner_id != 0u && owner_id == console_id;
    }

    [[nodiscard]] bool isHeartbeatExpired(uint32_t now_ms) const noexcept
    {
        if (owner_id == 0u) {
            return false;
        }
        return (now_ms - last_heartbeat_ms) > Dynamics::heartbeat_timeout_ms;
    }

    void release(uint64_t mask) noexcept
    {
        leased_mask &= ~mask;
        if (leased_mask == 0ull) {
            owner_id = 0u;
            last_heartbeat_ms = 0u;
        }
    }

    void releaseAll() noexcept
    {
        leased_mask = 0ull;
        owner_id = 0u;
        last_heartbeat_ms = 0u;
    }
};

/**
 * Попытка захвата маски.
 * @return true, если все запрошенные биты свободны или уже принадлежат console_id.
 */
[[nodiscard]] inline bool tryAcquireLease(LeaseSession& session,
                                          uint8_t console_id,
                                          uint64_t request_mask,
                                          uint32_t now_ms) noexcept
{
    const uint64_t foreign = session.leased_mask & ~request_mask;
    if (foreign != 0ull && session.owner_id != 0u && session.owner_id != console_id) {
        return false;
    }

    if (session.owner_id == 0u) {
        session.owner_id = console_id;
    } else if (session.owner_id != console_id) {
        return false;
    }

    session.leased_mask |= request_mask;
    session.last_heartbeat_ms = now_ms;
    return true;
}

} // namespace smcp
