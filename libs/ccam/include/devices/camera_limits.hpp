/**
 * @file camera_limits.hpp
 * @brief Диапазон gain в дБ для конкретной модели (передаётся в CameraDeviceBase).
 *
 * OGU: code = 0x08 + dB (ConvertibleProtocol.pdf). AGC = 0x80.
 * ORV (iris) одинаков для всех — живёт в CameraDeviceBase.
 */

#pragma once

namespace ccam::devices {

/** Линейный gain модели в дБ (не коды протокола). */
struct GainDbRange {
    int min_db;
    int max_db;
};

/** Студийные AW-E600 / E650 и др.: 0..18 dB. */
inline constexpr GainDbRange kGainDbStudio{0, 18};

/** AW-HE130 / UE150 / HR140 и др.: 0..48 dB. */
inline constexpr GainDbRange kGainDbHeWide{0, 48};

/** AW-HE870: −6..+18 dB. */
inline constexpr GainDbRange kGainDbHe870{-6, 18};

} // namespace ccam::devices
