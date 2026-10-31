/**
 * @file e650.hpp
 * @brief AW-E650 / AW-E650E — студийная камера (только camera protocol).
 *
 * ConvertibleProtocol.pdf v3.05, колонка E650.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "protocol/choice_lists.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** AWC — коды QAW/UI (0/2/3/4/5). setAwcMode шлёт OAW control. */
enum class E650AwcMode : uint8_t {
    CCAM_FOR_EACH_STUDIO_AWC(CCAM_ENUM)
};

enum class E650Scene : uint8_t {
    CCAM_FOR_EACH_SCENE(CCAM_ENUM)
};

/** OSH (PDF): Ah=1/10000, Bh=Synchro-Scan, Ch=ELC, Dh=1/24, Eh=1/25, Fh=1/30. */
enum class E650ShutterMode : uint8_t {
    CCAM_FOR_EACH_STUDIO_SHUTTER(CCAM_ENUM)
};

CCAM_DEFINE_U8_LABEL(E650AwcMode, CCAM_FOR_EACH_STUDIO_AWC)
CCAM_DEFINE_U8_LABEL(E650Scene, CCAM_FOR_EACH_SCENE)
CCAM_DEFINE_U8_LABEL(E650ShutterMode, CCAM_FOR_EACH_STUDIO_SHUTTER)

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
