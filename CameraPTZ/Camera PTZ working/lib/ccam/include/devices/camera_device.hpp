/**
 * @file camera_device.hpp
 * @brief Камера Convertible Protocol — протокол + модель в одном объекте.
 *
 * Кадр: [STX][команда][:data][ETX]. ConvertibleProtocol.pdf v3.05, Camera Control.
 * Pan/tilt/zoom — через PtDeviceBase (#P, #T, #Z, #F).
 */

#pragma once

#include "catalog/catalog.hpp"
#include "devices/device_types.hpp"
#include "transport/frame.hpp"
#include "protocol/protocol_encoding.hpp"
#include "transport/rs485_transport.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::devices {

/**
 * Полиморфный интерфейс камеры (для PtzOperator и UI).
 */
class ICameraDevice {
public:
    virtual ~ICameraDevice() = default;

    virtual CameraModelId modelId() const = 0;
    virtual const char* modelLabel() const = 0;

    virtual Status queryModel(char* out, size_t out_cap) = 0;
    virtual Status awcStart() = 0;
    virtual Status setAwcMode(uint8_t mode) = 0;
};

/**
 * Базовая камера: RS485-транспорт и все O/Q/OSD/меню.
 * Наследники добавляют modelId() и команды под модель.
 */
class CameraDeviceBase : public ICameraDevice {
public:
    explicit CameraDeviceBase(Rs485Transport& transport);

    CameraModelId modelId() const override { return CameraModelId::Generic; }
    const char* modelLabel() const override { return "Generic Convertible Camera"; }

    /* --- Сборка кадров (patterns 1–18) --- */
    Status buildTrigger(CameraFrame& frame, const char* cmd3);
    Status buildSet(CameraFrame& frame, const char* cmd3, const char* data);
    Status buildQuery(CameraFrame& frame, const char* cmd3);
    Status buildScene(CameraFrame& frame, const char* data);
    Status buildMonitor(CameraFrame& frame, const char* dxx, const char* data);
    Status buildOsdSet(CameraFrame& frame, uint8_t subcode, const char* data);
    Status buildOsdQuery(CameraFrame& frame, uint8_t subcode);
    Status buildLensH(CameraFrame& frame, const char* hxx);
    Status buildMenu(
        CameraFrame& frame,
        catalog::MenuBank bank,
        uint8_t subcode,
        const char* data,
        bool set);

    Status send(const CameraFrame& frame);
    Status transact(
        const CameraFrame& frame,
        uint8_t* response = nullptr,
        size_t response_cap = 0,
        size_t* response_len = nullptr,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

    Status apply(const catalog::CameraCommandEntry& entry, const char* data);
    Status queryEntry(
        const catalog::CameraCommandEntry& entry,
        char* out,
        size_t out_cap,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

    Status setOsd(uint8_t subcode, const char* data);
    Status queryOsd(uint8_t subcode, char* out, size_t out_cap, uint32_t timeout_ms = kDefaultRxTimeoutMs);
    Status setMenu(catalog::MenuBank bank, uint8_t subcode, const char* data);
    Status queryMenu(
        catalog::MenuBank bank,
        uint8_t subcode,
        char* out,
        size_t out_cap,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);
    Status selectScene(const char* data);
    Status setMonitor(const char* dxx, const char* data);
    Status lensContact(const char* hxx);

    Status queryModel(char* out, size_t out_cap) override;
    Status querySoftwareVersion(char* out, size_t out_cap);

    Status awcStart() override;
    Status abcStart();
    Status setAwcMode(uint8_t mode) override;

    Status setShutter(const char* data_hex);
    Status setGain(const char* data_hex);
    Status setDetail(uint8_t mode);
    Status setHdDetail(uint8_t mode);
    Status setIrisAutoManual(uint8_t mode);
    Status setIrisAutoManual(IrisAutoManual mode);
    Status setManualIris(const char* data_hex);
    Status setSceneFile(uint8_t scene);
    Status setColorBar(uint8_t mode);
    Status setMenuOnOff(uint8_t mode);
    Status setMenuOnOff(bool on);
    Status setAutoFocus(uint8_t mode);
    Status setAutoFocus(bool on);
    Status setFilter(uint8_t mode);
    Status setDec1Cmd(const char* cmd3, uint8_t value);

    Status triggerCmd(const char* cmd3);
    Status setCmd(const char* cmd3, const char* data);
    Status queryCmd(const char* cmd3, char* out, size_t out_cap);

    Status setOsd(catalog::OsdItem item, const char* data);
    Status queryOsd(catalog::OsdItem item, char* out, size_t out_cap);
    Status setVideoMenu(catalog::VideoMenuItem item, const char* data);
    Status queryVideoMenu(catalog::VideoMenuItem item, char* out, size_t out_cap);
    Status setExtendedJMenu(catalog::ExtendedJItem item, const char* data);
    Status setExtendedJMenuSwitch(catalog::ExtendedJItem item, ProtocolSwitch sw);
    Status setVideoMenuSwitch(catalog::VideoMenuItem item, ProtocolSwitch sw);
    Status queryExtendedJMenu(catalog::ExtendedJItem item, char* out, size_t out_cap);

    Status apply(catalog::CameraCmd cmd, const char* data);
    Status applyDec1(catalog::CameraCmd cmd, uint8_t value);
    Status applySwitch(catalog::CameraCmd cmd, ProtocolSwitch sw);
    Status applyTrigger(catalog::CameraCmd cmd);
    Status query(catalog::CameraCmd cmd, char* out, size_t out_cap);

protected:
    Rs485Transport& transport_;

    Status waitAck(uint32_t timeout_ms);
    Status copyResponsePayload(
        const uint8_t* response,
        size_t response_len,
        char* out,
        size_t out_cap);
};

} // namespace ccam::devices
