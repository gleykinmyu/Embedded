/**
 * @file protocol_values.hpp
 * @brief Значения протокола, общие для нескольких моделей камер/головок.
 */

#pragma once

#include <cstdint>

namespace ccam::devices {

/** OCG — Chroma Level 0..9. */
enum class ChromaLevel : uint8_t {
    Level0 = 0,
    Level1 = 1,
    Level2 = 2,
    Level3 = 3,
    Level4 = 4,
    Level5 = 5,
    Level6 = 6,
    Level7 = 7,
    Level8 = 8,
    Level9 = 9,
};

} // namespace ccam::devices
