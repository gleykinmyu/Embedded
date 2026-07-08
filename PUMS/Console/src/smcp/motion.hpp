/**
 * @file motion.hpp
 * @brief Параметры движения и взвода SMCP v1.5 (блоки 1.5, 2.6, 5.3).
 */

#pragma once

#include <cstdint>

#include "constants.hpp"

namespace smcp {

/** Параметры индивидуального движения (MOVE_INDIV / Arming). */
struct MoveParams {
    int32_t target_mm = 0;
    uint16_t speed_mm_s = 0;
    uint16_t accel_mm_s2 = Dynamics::default_accel_mm_s2;
};

/** Команда синхронного старта (MOVE_SYNC) — только при нажатой кнопке СТАРТ. */
struct SyncStartCommand {
    uint64_t start_mask = 0;
    bool start_button_pressed = false;
};

enum class StopMode : uint8_t {
    Smooth,
    Emergency,
};

struct StopCommand {
    uint64_t stop_mask = 0;
    StopMode mode = StopMode::Smooth;
};

/** Этапы синхронного старта сегмента (блок 5.3). */
enum class SyncStartStage : uint8_t {
    Prepare,
    Verify,
    Go,
};

} // namespace smcp
