/**
 * @file ue150.hpp
 * @brief AW-UE150 / UE155 / UN145 — PTZ-камера.
 *
 * ConvertibleProtocol.pdf v3.05, колонки UE150/UE155/UN145.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/integrated_ptz.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

enum class Ue150AwcMode : uint8_t {
    Atw = 0,
    AwcA = 1,
    AwcB = 2,
    Preset3200K = 3,
    Preset5600K = 4,
    Var = 5,
};

enum class Ue150Scene : uint8_t {
    Scene1 = 0,
    Scene2 = 1,
    Scene3 = 2,
    Scene4 = 3,
};

/** Камера AW-UE150 (и совместимые UE155, UN145). */
class Ue150Camera : public CameraDeviceBase {
public:
    explicit Ue150Camera(Transport& transport)
        : CameraDeviceBase(transport, kGainDbHeWide)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::Ue150; }
    const char* modelLabel() const override { return "AW-UE150"; }

    Status setAwcMode(Ue150AwcMode mode)
    {
        return CameraDeviceBase::setAwcMode(static_cast<uint8_t>(mode));
    }

    Status selectScene(Ue150Scene scene)
    {
        char data[2] = {static_cast<char>('0' + static_cast<uint8_t>(scene)), '\0'};
        return CameraDeviceBase::selectScene(data);
    }

    /** QSA:60 / OSA:60 MODE @S.GAIN. */
    Status setSuperGain(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::ModeAtSuperGain, data);
    }

    /** QSJ:26 / OSJ:26 HDMI Out HDR Output Select. */
    Status setHdmiOutHdrOutputSelect(const char* data)
    {
        return setExtendedJMenu(catalog::ExtendedJItem::HdmiOutHdrOutputSelect, data);
    }

    /** QSJ:3D / OSJ:3D Zoom Scale. */
    Status setZoomScaleDisplay(const char* data)
    {
        return setExtendedJMenu(catalog::ExtendedJItem::ZoomScale, data);
    }

    /** QSJ:40 / OSJ:40 Operation Lock Status. */
    Status setOperationLock(bool locked)
    {
        return setExtendedJMenuSwitch(
            catalog::ExtendedJItem::OperationLockStatus, toSwitch(locked));
    }
};

/** Встроенный pan/tilt AW-UE150. */
class Ue150Pt : public IntegratedPtzBase {
public:
    static constexpr uint8_t kMaxPreset = 99;

    explicit Ue150Pt(Transport& transport)
        : IntegratedPtzBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ue150; }
    const char* modelLabel() const override { return "AW-UE150 Pan/Tilt"; }

    Status setNdFilter(PtNdFilterMode mode)
    {
        return sendDec1(catalog::PtCmd::NdFilter, static_cast<uint8_t>(mode));
    }
};

} // namespace ccam::devices
