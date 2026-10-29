/**
 * @file he870.hpp
 * @brief AW-HE870 — PTZ-камера (camera + integrated P/T).
 *
 * ConvertibleProtocol.pdf v3.05, колонка HE870.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/integrated_ptz.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** Режим AWC (OAW), HE870. */
enum class He870AwcMode : uint8_t {
    Atw = 0,
    AwcA = 1,
    AwcB = 2,
    Preset3200K = 3,
    Preset5600K = 4,
    Var = 5,
};

/** Сцена (OSF / XSF). */
enum class He870Scene : uint8_t {
    Halogen = 0,
    Fluorescent = 1,
    Outdoor = 2,
    User = 3,
};

/** SC Coarse (OSC) — HE870 поддерживает 45° шаги (OSC 5..8). */
enum class He870ScCoarse : uint8_t {
    Deg0 = 1,
    Deg90 = 2,
    Deg180 = 3,
    Deg270 = 4,
    Deg45 = 5,
    Deg135 = 6,
    Deg225 = 7,
    Deg315 = 8,
};

/** OAR — Light Area (A.IRIS). */
enum class He870LightArea : uint8_t {
    All = 0,
    Center = 1,
    TopCut = 5,
    Bottom = 6,
    LeftRight = 7,
};

/** ND / фильтр (OFT), HE870. */
enum class He870NdFilter : uint8_t {
    Clear = 0,
    NdQuarter = 1,
    NdSixteenth = 2,
    NdSixtyFourth = 3,
    AutoNd = 8,
};

/**
 * Камера AW-HE870.
 *
 * Gain: -6..+18 dB (см. PDF OGU). SC Coarse — специальные углы 45°.
 */
class He870Camera : public CameraDeviceBase {
public:
    explicit He870Camera(Transport& transport)
        : CameraDeviceBase(transport, kGainDbHe870)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::He870; }
    const char* modelLabel() const override { return "AW-HE870"; }

    Status setAwcMode(He870AwcMode mode)
    {
        return CameraDeviceBase::setAwcMode(static_cast<uint8_t>(mode));
    }

    Status selectScene(He870Scene scene)
    {
        char data[2] = {static_cast<char>('0' + static_cast<uint8_t>(scene)), '\0'};
        return CameraDeviceBase::selectScene(data);
    }

    /** SC Coarse — только control command на HE870 (QSC может отличаться). */
    Status setScCoarse(He870ScCoarse value)
    {
        return sendDec1(catalog::CameraCmd::ScCoarse, static_cast<uint8_t>(value));
    }

    Status setNdFilter(He870NdFilter filter)
    {
        const uint8_t v = static_cast<uint8_t>(filter);
        if (v >= 10) {
            char data[3];
            if (encodeDecAscii(v, data, sizeof(data)) != Status::Ok) {
                return Status::Param;
            }
            return send(catalog::CameraCmd::FilterNd, data);
        }
        return sendDec1(catalog::CameraCmd::FilterNd, v);
    }

    /** Digital extender x1.4 (ODE). */
    Status setDigitalExtender(bool on)
    {
        return sendDec1(catalog::CameraCmd::DigitalExtender, static_cast<uint8_t>(toSwitch(on)));
    }

    /** A.IRIS Light Area (OAR). */
    Status setLightArea(He870LightArea area)
    {
        return sendDec1(catalog::CameraCmd::LightArea, static_cast<uint8_t>(area));
    }

    /** OSD:48 A.IRIS Level / IRIS Offset. */
    Status setAirisLevel(const char* data)
    {
        return setOsd(catalog::OsdItem::AirisLevel, data);
    }

    /** Video menu: MODE @S.GAIN (QSA:60 / OSA:60, PDF стр. 14). */
    Status setSuperGain(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::ModeAtSuperGain, data);
    }
};

/** Встроенный pan/tilt AW-HE870. */
class He870Pt : public IntegratedPtzBase {
public:
    static constexpr uint8_t kMaxPreset = 99;

    explicit He870Pt(Transport& transport)
        : IntegratedPtzBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::He870; }
    const char* modelLabel() const override { return "AW-HE870 Pan/Tilt"; }

    Status setNdFilter(He870NdFilter filter)
    {
        return sendDec1(catalog::PtCmd::NdFilter, static_cast<uint8_t>(filter));
    }

    Status setAutoFocusAux(bool on)
    {
        return sendDec1(catalog::PtCmd::ExtenderAf, static_cast<uint8_t>(toSwitch(on)));
    }
};

} // namespace ccam::devices
