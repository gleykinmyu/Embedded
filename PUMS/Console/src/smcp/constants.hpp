/**
 * @file constants.hpp
 * @brief Константы подсистемы механики StageNet SMCP v1.5.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace smcp {

/** Ёмкость сегмента и адресация серверов (блок 3.1). */
struct Segment {
    static inline constexpr uint8_t mechs_per_server = 64u;
    static inline constexpr uint8_t max_presets_per_server = 64u;
    /** Сервер 0x10 обслуживает Global ID 0..63, 0x11 — 64..127 и т.д. */
    static inline constexpr uint8_t server_id_base = 0x10u;
};

/** Размеры wire-структур и строковых полей (блок 3.3). */
struct Layout {
    static inline constexpr std::size_t mech_config = 32u;
    static inline constexpr std::size_t group = 64u;
    static inline constexpr std::size_t preset = 320u;
    static inline constexpr std::size_t group_name = 48u;
    static inline constexpr std::size_t preset_name = 32u;
};

/** Динамика, синхронизация и тайминги сессии (блок 2.6). */
struct Dynamics {
    static inline constexpr uint16_t default_accel_mm_s2 = 50u;
    static inline constexpr uint16_t slowdown_dist_mm = 150u;
    static inline constexpr uint16_t brake_delay_ms = 200u;
    static inline constexpr uint16_t sync_tolerance_mm = 20u;
    static inline constexpr uint16_t heartbeat_timeout_ms = 500u;
};

/** Пороги независимого контроля Checker (блок 2.3). */
struct Safety {
    static inline constexpr uint8_t cross_check_pos_tolerance_pct = 1u;
};

} // namespace smcp
