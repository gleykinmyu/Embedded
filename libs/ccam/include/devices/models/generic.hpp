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
    explicit GenericCamera(Rs485Transport& transport)
        : CameraDeviceBase(transport)
    {
    }

    CameraModelId modelId() const override { return CameraModelId::Generic; }
    const char* modelLabel() const override { return "Generic Convertible Camera"; }
};

/** Любое поворотное устройство / PTZ (базовый набор #-команд). */
class GenericPt : public PtDeviceBase {
public:
    explicit GenericPt(Rs485Transport& transport)
        : PtDeviceBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Generic; }
    const char* modelLabel() const override { return "Generic Pan/Tilt"; }
};

} // namespace ccam::devices
