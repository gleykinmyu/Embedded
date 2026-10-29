/**
 * @file e650.hpp
 * @brief AW-E650 / AW-E650E — студийная камера (только camera protocol).
 *
 * ConvertibleProtocol.pdf v3.05, колонка E650.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** AWC mode (OAW), E650. */
enum class E650AwcMode : uint8_t {
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

enum class E650Scene : uint8_t {
    Halogen = 0,
    Fluorescent = 1,
    Outdoor = 2,
    User = 3,
};

enum class E650ShutterMode : uint8_t {
    Off = 0,
    Shutter1_50 = 1,
    Shutter1_60 = 2,
    Shutter1_100_120 = 3,
    Shutter1_250 = 5,
    SynchroScan = 0x0A,
    Elc = 0x0B,
};

/**
 * Студийная камера AW-E650 / AW-E650E.
 */
class E650Camera : public CameraDeviceBase {
public:
    explicit E650Camera(Transport& transport)
        : CameraDeviceBase(transport, kGainDbStudio)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::E650; }
    const char* modelLabel() const override { return "AW-E650E"; }

    Status setAwcMode(E650AwcMode mode)
    {
        return CameraDeviceBase::setAwcMode(static_cast<uint8_t>(mode));
    }

    Status selectScene(E650Scene scene)
    {
        return setSceneFile(static_cast<uint8_t>(scene));
    }

    Status setShutterMode(E650ShutterMode mode)
    {
        return CameraDeviceBase::setShutterMode(static_cast<uint8_t>(mode));
    }
};

} // namespace ccam::devices
