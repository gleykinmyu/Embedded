/**
 * @file types.hpp
 * @brief Структуры данных механики SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "constants.hpp"

namespace smcp {

#pragma pack(push, 1)

/** Пара границ [Limit_1, Limit_2] в миллиметрах. */
struct Limits {
    int32_t limit1 = 0;
    int32_t limit2 = 0;
};

/**
 * Паспорт физики механизма — 32 байта.
 * Дублируется в Master и Checker (Golden Config).
 */
struct MechConfig {
    uint16_t id = 0;
    uint8_t type = 0;
    uint8_t options = 0;
    Limits hard_limits{};
    Limits soft_limits{};
    uint16_t max_speed = 0;
    uint32_t pulses_per_m = 0;
    uint16_t crc16 = 0;
    uint16_t padding = 0;
    uint8_t _wire_pad[2]{}; /**< Доводка блока до 32 байт (DMA / Flash). */
};

static_assert(sizeof(MechConfig) == Layout::mech_config);

/**
 * Статический снимок позиций сегмента сервера — 320 байт.
 */
struct Preset {
    uint16_t id = 0;
    char name[Layout::preset_name]{};
    int32_t positions[Segment::mechs_per_server]{};
    uint16_t default_fade = 0;
    uint16_t crc16 = 0;
    uint8_t reserved[26]{};
};

static_assert(sizeof(Preset) == Layout::preset);

#pragma pack(pop)

/** Оперативный контекст выбора штанкетов консолью (блок 3.4). */
struct Selection {
    uint64_t active_mask = 0;
    uint8_t owner_id = 0;
    bool sync_status = false;
};

} // namespace smcp
