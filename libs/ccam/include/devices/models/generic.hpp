/**
 * @file generic.hpp
 * @brief Универсальная камера и P/T без модель-специфичных методов.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/pt_device.hpp"

namespace ccam::devices {

/** Любая камера Convertible Protocol (базовый набор команд). */
class GenericCamera : public CameraDeviceBase {
public:
    explicit GenericCamera(Transport& transport)
        : CameraDeviceBase(transport, kGainDbStudio)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::Generic; }
    const char* modelLabel() const override { return "Generic Convertible Camera"; }
};

/** Любое поворотное устройство / PTZ (базовый набор #-команд). */
class GenericPt : public PtDeviceBase {
public:
    explicit GenericPt(Transport& transport)
        : PtDeviceBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Generic; }
    const char* modelLabel() const override { return "Generic Pan/Tilt"; }

    Status power(pt::PowerMode mode) override
    {
        switch (mode) {
        case pt::PowerMode::Off:
            return sendDec1(catalog::PtCmd::Power, 0);
        case pt::PowerMode::On:
            return sendDec1(catalog::PtCmd::Power, 1);
        default:
            return Status::Param;
        }
    }
};

} // namespace ccam::devices
