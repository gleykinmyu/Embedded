/**
 * @file he870.hpp
 * @brief AW-HE870 — PTZ-камера (camera + integrated P/T).
 *
 * ConvertibleProtocol.pdf v3.05, колонка HE870.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/integrated_ptz.hpp"
#include "protocol/choice_lists.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** Режим AWC (OAW), HE870. Тот же список, что HE130 и UE150. */
enum class He870AwcMode : uint8_t {
    CCAM_FOR_EACH_HE_AWC(CCAM_ENUM)
};

/** Сцена (OSF / XSF). */
enum class He870Scene : uint8_t {
    CCAM_FOR_EACH_SCENE(CCAM_ENUM)
};

/** SC Coarse (OSC) — HE870 поддерживает 45° шаги (OSC 5..8). */
enum class He870ScCoarse : uint8_t {
    CCAM_FOR_EACH_SC_COARSE(CCAM_ENUM)
};

/** Light Area A.IRIS (Control ORA, reply OAR). */
enum class He870LightArea : uint8_t {
    CCAM_FOR_EACH_LIGHT_AREA(CCAM_ENUM)
};

/** ND / фильтр (OFT), HE870. Те же коды, что PtNdFilterMode. */
enum class He870NdFilter : uint8_t {
    CCAM_FOR_EACH_ND(CCAM_ENUM)
};

CCAM_DEFINE_U8_LABEL(He870AwcMode, CCAM_FOR_EACH_HE_AWC)
CCAM_DEFINE_U8_LABEL(He870Scene, CCAM_FOR_EACH_SCENE)
CCAM_DEFINE_U8_LABEL(He870ScCoarse, CCAM_FOR_EACH_SC_COARSE)
CCAM_DEFINE_U8_LABEL(He870LightArea, CCAM_FOR_EACH_LIGHT_AREA)
CCAM_DEFINE_U8_LABEL(He870NdFilter, CCAM_FOR_EACH_ND)

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
        return setSceneFile(static_cast<uint8_t>(scene));
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

    /** A.IRIS Light Area (Control ORA, reply OAR). */
    Status setLightArea(He870LightArea area)
    {
        return sendDec1(catalog::CameraCmd::LightArea, static_cast<uint8_t>(area));
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
