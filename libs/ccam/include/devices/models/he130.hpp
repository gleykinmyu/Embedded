/**
 * @file he130.hpp
 * @brief AW-HE130 — PTZ-камера (camera + integrated P/T).
 *
 * ConvertibleProtocol.pdf v3.05, колонки HE130.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/integrated_ptz.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** Режим AWC для HE130 (OAW:data, см. PDF). */
enum class He130AwcMode : uint8_t {
    Atw = 0,
    AwcA = 1,
    AwcB = 2,
    Preset3200K = 3,
    Preset5600K = 4,
    Var = 5,
};

/** Сцена (OSF / XSF), HE130. */
enum class He130Scene : uint8_t {
    Halogen = 0,
    Fluorescent = 1,
    Outdoor = 2,
    User = 3,
};

/**
 * Камера AW-HE130.
 *
 * @code
 * ccam::Transport bus(hal);
 * ccam::devices::He130Camera camera(bus);
 * camera.setAwcMode(He130AwcMode::Atw);
 * camera.setSuperGain("02");
 * @endcode
 */
class He130Camera : public CameraDeviceBase {
public:
    explicit He130Camera(Transport& transport)
        : CameraDeviceBase(transport, kGainDbHeWide)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::He130; }
    const char* modelLabel() const override { return "AW-HE130"; }

    Status setAwcMode(He130AwcMode mode)
    {
        return CameraDeviceBase::setAwcMode(static_cast<uint8_t>(mode));
    }

    Status selectScene(He130Scene scene)
    {
        char data[2] = {static_cast<char>('0' + static_cast<uint8_t>(scene)), '\0'};
        return CameraDeviceBase::selectScene(data);
    }

    /** QSA:60 / OSA:60 MODE @S.GAIN (Video menu, PDF стр. 14). */
    Status setSuperGain(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::ModeAtSuperGain, data);
    }

    /** QSD:72 / OSD:72 ATW Speed (User OSD). */
    Status setAtwSpeed(const char* data)
    {
        return setOsd(catalog::OsdItem::AtwSpeed, data);
    }

    /** QSJ:26 / OSJ:26 HDMI Out HDR Output Select. */
    Status setHdmiOutHdrOutputSelect(const char* data)
    {
        return setExtendedJMenu(catalog::ExtendedJItem::HdmiOutHdrOutputSelect, data);
    }

    /** QSJ:45 / OSJ:45 Power On Position. */
    Status setPowerOnPosition(const char* data)
    {
        return setExtendedJMenu(catalog::ExtendedJItem::PowerOnPosition, data);
    }

    /** QSJ:46 / OSJ:46 Power On Preset Number. */
    Status setPowerOnPreset(const char* data)
    {
        return setExtendedJMenu(catalog::ExtendedJItem::PowerOnPresetNumber, data);
    }

    /** 3D-DNR (ODD). */
    Status set3dDnr(Levels level)
    {
        return sendDec1(catalog::CameraCmd::Dnr3d, static_cast<uint8_t>(level));
    }

    /** Digital extender x1.4 (ODE). */
    Status setDigitalExtender(bool on)
    {
        return sendDec1(catalog::CameraCmd::DigitalExtender, static_cast<uint8_t>(toSwitch(on)));
    }
};

/**
 * Встроенный pan/tilt AW-HE130 (та же RS485-линия, #-протокол).
 */
class He130Pt : public IntegratedPtzBase {
public:
    static constexpr uint8_t kMaxPreset = 99;

    explicit He130Pt(Transport& transport)
        : IntegratedPtzBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::He130; }
    const char* modelLabel() const override { return "AW-HE130 Pan/Tilt"; }

    /** ND filter (#D2). */
    Status setNdFilter(PtNdFilterMode mode)
    {
        return sendDec1(catalog::PtCmd::NdFilter, static_cast<uint8_t>(mode));
    }

    /** Встроенный AF on/off (#D1), не OAF камеры. */
    Status setAutoFocusAux(bool on)
    {
        return sendDec1(catalog::PtCmd::ExtenderAf, static_cast<uint8_t>(toSwitch(on)));
    }
};

} // namespace ccam::devices
