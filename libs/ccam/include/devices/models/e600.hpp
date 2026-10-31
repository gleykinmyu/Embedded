/**
 * @file e600.hpp
 * @brief AW-E600 / AW-E600E — студийная камера (только camera protocol).
 *
 * ConvertibleProtocol.pdf v3.05, колонка E600.
 * Pan/tilt — отдельное устройство (напр. AW-PH350E).
 */

#pragma once

#include "devices/camera_device.hpp"
#include "protocol/choice_lists.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** AWC — коды QAW/UI (как E650). setAwcMode шлёт OAW control. */
enum class E600AwcMode : uint8_t {
    CCAM_FOR_EACH_STUDIO_AWC(CCAM_ENUM)
};

/** Scene file (OSF). */
enum class E600Scene : uint8_t {
    CCAM_FOR_EACH_SCENE(CCAM_ENUM)
};

/** OSH — как у E650. */
enum class E600ShutterMode : uint8_t {
    CCAM_FOR_EACH_STUDIO_SHUTTER(CCAM_ENUM)
};

CCAM_DEFINE_U8_LABEL(E600AwcMode, CCAM_FOR_EACH_STUDIO_AWC)
CCAM_DEFINE_U8_LABEL(E600Scene, CCAM_FOR_EACH_SCENE)
CCAM_DEFINE_U8_LABEL(E600ShutterMode, CCAM_FOR_EACH_STUDIO_SHUTTER)

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
