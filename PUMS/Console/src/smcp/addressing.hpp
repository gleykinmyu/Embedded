/**
 * @file addressing.hpp
 * @brief Сквозная адресация механизмов SMCP v1.5 (блок 3.1).
 */

#pragma once

#include <cstdint>

#include "constants.hpp"

namespace smcp {

/** Global ID (0..65535) → адрес сервера (0x10..). */
[[nodiscard]] constexpr uint8_t globalToServerId(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(Segment::server_id_base + (global_id / Segment::mechs_per_server));
}

/** Global ID → локальный индекс бита в маске сервера (0..63). */
[[nodiscard]] constexpr uint8_t globalToLocalId(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(global_id % Segment::mechs_per_server);
}

/** Сервер + локальный индекс → Global ID. */
[[nodiscard]] constexpr uint16_t serverLocalToGlobalId(uint8_t server_id, uint8_t local_id) noexcept
{
    const uint16_t segment = static_cast<uint16_t>(server_id - Segment::server_id_base) * Segment::mechs_per_server;
    return static_cast<uint16_t>(segment + local_id);
}

/** Бит локального механизма в uint64_t-маске сегмента. */
[[nodiscard]] constexpr uint64_t localBit(uint8_t local_id) noexcept
{
    return (local_id < Segment::mechs_per_server) ? (1ull << local_id) : 0ull;
}

/** Проверка участия Global ID в маске своего сервера. */
[[nodiscard]] constexpr bool maskContains(uint64_t mask, uint16_t global_id) noexcept
{
    return (mask & localBit(globalToLocalId(global_id))) != 0ull;
}

} // namespace smcp
