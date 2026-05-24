/**
 * @file e600.hpp
 * @brief AW-E600 / AW-E600E — студийная камера (только camera protocol).
 *
 * ConvertibleProtocol.pdf v3.05, колонка E600.
 * Pan/tilt — отдельное устройство (напр. AW-PH350E).
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/protocol_values.hpp"
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
 *
 * @code
 * ccam::devices::E600Camera camera(bus);
 * camera.setAwcMode(E600AwcMode::Preset3200K);
 * camera.setSynchroScan("001");
 * @endcode
 */
class E600Camera : public CameraDeviceBase {
public:
    explicit E600Camera(Rs485Transport& transport)
        : CameraDeviceBase(transport)
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
        char data[3];
        const uint8_t v = static_cast<uint8_t>(mode);
        if (v >= 10) {
            data[0] = static_cast<char>('0' + (v / 10));
            data[1] = static_cast<char>('0' + (v % 10));
            data[2] = '\0';
        } else {
            data[0] = static_cast<char>('0' + v);
            data[1] = '\0';
        }
        return setShutter(data);
    }

    /** Synchro Scan (OMS) — hex/data по PDF, напр. "001".."0FF". */
    Status setSynchroScan(const char* data)
    {
        return apply(catalog::CameraCmd::SynchroScan, data);
    }

    Status setNegPos(bool negative)
    {
        return applySwitch(
            catalog::CameraCmd::NegPos,
            negative ? ProtocolSwitch::On : ProtocolSwitch::Off);
    }

    Status setFieldFrame(FieldFrameMode mode)
    {
        return applyDec1(catalog::CameraCmd::FieldFrame, static_cast<uint8_t>(mode));
    }

    /** Chroma Level (OCG). */
    Status setChromaLevel(ChromaLevel level)
    {
        return applyDec1(catalog::CameraCmd::ChromaLevel, static_cast<uint8_t>(level));
    }

    /** R/B Gain и Pedestal — data в формате протокола (hex). */
    Status setRGain(const char* data) { return apply(catalog::CameraCmd::RGain, data); }
    Status setBGain(const char* data) { return apply(catalog::CameraCmd::BGain, data); }
    Status setRPedestal(const char* data) { return apply(catalog::CameraCmd::RPedestal, data); }
    Status setBPedestal(const char* data) { return apply(catalog::CameraCmd::BPedestal, data); }

    /** User OSD: Gamma (QSD:00). */
    Status setGamma(const char* data) { return setOsd(catalog::OsdItem::Gamma, data); }
    Status setAgcMax(const char* data) { return setOsd(catalog::OsdItem::AgcMax, data); }

    /** Video/Film paint menu (OSA/QSA). */
    Status setTotalDtlLevel(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::TotalDtlLevel, data);
    }

    Status setGenlockInput(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::GenlockInput, data);
    }
};

} // namespace ccam::devices
