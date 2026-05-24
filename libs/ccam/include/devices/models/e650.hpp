/**
 * @file e650.hpp
 * @brief AW-E650 / AW-E650E — студийная камера (только camera protocol).
 *
 * ConvertibleProtocol.pdf v3.05, колонка E650.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/protocol_values.hpp"
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
 *
 * Отличия от E600 минимальны в протоколе; отдельный класс для явного выбора модели.
 */
class E650Camera : public CameraDeviceBase {
public:
    explicit E650Camera(Rs485Transport& transport)
        : CameraDeviceBase(transport)
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

    /** ABC/ABB balance (OAS) — поддерживается E650. */
    Status abcBalanceStart()
    {
        return abcStart();
    }

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

    Status setChromaLevel(ChromaLevel level)
    {
        return applyDec1(catalog::CameraCmd::ChromaLevel, static_cast<uint8_t>(level));
    }

    Status setRGain(const char* data) { return apply(catalog::CameraCmd::RGain, data); }
    Status setBGain(const char* data) { return apply(catalog::CameraCmd::BGain, data); }
    Status setRPedestal(const char* data) { return apply(catalog::CameraCmd::RPedestal, data); }
    Status setBPedestal(const char* data) { return apply(catalog::CameraCmd::BPedestal, data); }

    Status setGamma(const char* data) { return setOsd(catalog::OsdItem::Gamma, data); }
    Status setAgcMax(const char* data) { return setOsd(catalog::OsdItem::AgcMax, data); }

    Status setTotalDtlLevel(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::TotalDtlLevel, data);
    }

    Status setGenlockInput(const char* data)
    {
        return setVideoMenu(catalog::VideoMenuItem::GenlockInput, data);
    }

    /** Black Shading Correct DIG (OSA:C0). */
    Status setBlackShadingCorrect(bool on)
    {
        return setVideoMenuSwitch(
            catalog::VideoMenuItem::BlackShadingCorrectDig, toProtocolSwitch(on));
    }
};

} // namespace ccam::devices
