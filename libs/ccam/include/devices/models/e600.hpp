/**
 * @file e600.hpp
 * @brief AW-E600 / AW-E600E — студийная камера (только camera protocol).
 *
 * ConvertibleProtocol.pdf v3.05, колонка E600.
 * Pan/tilt — отдельное устройство (напр. AW-PH350E).
 */

#pragma once

#include "devices/camera_device.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** AWC mode (OAW), E600 — полный набор 0..9. */
enum class E600AwcMode : uint8_t {
    Atw = 0,
    AwcA = 1,
    AwcB = 2,
    Preset3200K = 3,
    Preset5600K = 4,
    Preset4500K = 5,
    Preset6000K = 6,
    Preset2800K = 7,
    Var = 8,
    Mode9 = 9,
};

/** Scene file (OSF). */
enum class E600Scene : uint8_t {
    Halogen = 0,
    Fluorescent = 1,
    Outdoor = 2,
    User = 3,
};

/** Shutter mode (OSH) — типичные значения E600, см. PDF. */
enum class E600ShutterMode : uint8_t {
    Off = 0,
    Shutter1_50 = 1,
    Shutter1_60 = 2,
    Shutter1_100_120 = 3,
    Shutter1_250 = 5,
    SynchroScan = 0x0A,
    Elc = 0x0B,
};

/**
 * Студийная камера AW-E600.
 */
class E600Camera : public CameraDeviceBase {
public:
    explicit E600Camera(Transport& transport)
        : CameraDeviceBase(transport, kGainDbStudio)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::E600; }
    const char* modelLabel() const override { return "AW-E600"; }

    Status setAwcMode(E600AwcMode mode)
    {
        return CameraDeviceBase::setAwcMode(static_cast<uint8_t>(mode));
    }

    Status selectScene(E600Scene scene)
    {
        return setSceneFile(static_cast<uint8_t>(scene));
    }

    Status setShutterMode(E600ShutterMode mode)
    {
        return CameraDeviceBase::setShutterMode(static_cast<uint8_t>(mode));
    }
};

} // namespace ccam::devices
