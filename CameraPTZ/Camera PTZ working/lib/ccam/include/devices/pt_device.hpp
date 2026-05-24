/**
 * @file pt_device.hpp
 * @brief Pan/Tilt Convertible Protocol — протокол + модель в одном объекте.
 *
 * ConvertibleProtocol.pdf v3.05, P/T Control (#…CR).
 */

#pragma once

#include "catalog/catalog.hpp"
#include "devices/device_types.hpp"
#include "transport/frame.hpp"
#include "protocol/protocol_encoding.hpp"
#include "protocol/pt_types.hpp"
#include "transport/rs485_transport.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::devices {

/**
 * Полиморфный интерфейс pan/tilt (для PtzOperator и UI).
 */
class IPtDevice {
public:
    virtual ~IPtDevice() = default;

    virtual PtModelId modelId() const = 0;
    virtual const char* modelLabel() const = 0;

    virtual Status power(PtPowerMode mode) = 0;
    virtual Status stopAll() = 0;

    virtual Status setPanTiltSpeed(uint8_t pan_speed, uint8_t tilt_speed) = 0;
    virtual Status panStop() = 0;
    virtual Status tiltStop() = 0;
    virtual Status setZoomSpeed(uint8_t speed) = 0;
    virtual Status setFocusSpeed(uint8_t speed) = 0;
    virtual Status zoomStop() = 0;
    virtual Status focusStop() = 0;

    virtual Status recallPreset(uint8_t preset) = 0;
    virtual Status savePreset(uint8_t preset) = 0;
    virtual Status queryPosition(PtPosition& position) = 0;
};

/**
 * Базовое поворотное устройство: RS485 и все #-команды PDF.
 */
class PtDeviceBase : public IPtDevice {
public:
    explicit PtDeviceBase(Rs485Transport& transport);

    PtModelId modelId() const override { return PtModelId::Generic; }
    const char* modelLabel() const override { return "Generic Pan/Tilt"; }

    static uint8_t speedForDirection(PtAxisDir dir, uint8_t rate_1_to_49);

    Status setPanSpeed(uint8_t speed);
    Status setTiltSpeed(uint8_t speed);
    Status setZoomSpeed(uint8_t speed) override;
    Status setFocusSpeed(uint8_t speed) override;
    Status setRollSpeed(uint8_t speed);
    Status setIrisSpeed(uint8_t speed);

    Status panStop() override;
    Status tiltStop() override;
    Status zoomStop() override;
    Status focusStop() override;
    Status rollStop();
    Status irisStop();
    Status stopAll() override;

    Status movePan(PtAxisDir dir, uint8_t rate_1_to_49);
    Status moveTilt(PtAxisDir dir, uint8_t rate_1_to_49);
    Status moveZoom(PtAxisDir dir, uint8_t rate_1_to_49);
    Status moveFocus(PtAxisDir dir, uint8_t rate_1_to_49);
    Status moveRoll(PtAxisDir dir, uint8_t rate_1_to_49);
    Status moveIris(PtAxisDir dir, uint8_t rate_1_to_49);

    Status setPanTiltSpeed(uint8_t pan_speed, uint8_t tilt_speed) override;
    Status power(PtPowerMode mode) override;

    Status recallPreset(uint8_t preset) override;
    Status savePreset(uint8_t preset) override;
    Status deletePreset(uint8_t preset);
    Status setPresetMode(uint8_t mode);
    Status setPresetSpeedTable(uint8_t table);

    Status absolutePosition(uint16_t pan, uint16_t tilt);
    Status absolutePositionWithSpeed(
        uint16_t pan,
        uint16_t tilt,
        uint8_t preset_speed,
        uint8_t speed_table);
    Status relativePosition(uint16_t pan, uint16_t tilt);
    Status relativePositionWithSpeed(
        uint16_t pan,
        uint16_t tilt,
        uint8_t preset_speed,
        uint8_t speed_table);

    Status sendCatalogDec1(catalog::PtCmd cmd, uint8_t value);
    Status sendCatalogSwitch(catalog::PtCmd cmd, ProtocolSwitch sw);
    Status setZoomPosition(uint16_t pos);
    Status setFocusPosition(uint16_t pos);
    Status setIrisPosition(uint16_t pos);
    Status queryZoomPosition(uint16_t& pos);
    Status queryFocusPosition(uint16_t& pos);
    Status queryIrisPosition(uint16_t& pos);
    Status setLensPositionCtrl(uint8_t mode);

    Status queryPosition(PtPosition& position) override;
    Status queryPositionDisplay(PtPosition& position);
    Status queryError(uint8_t& error_code);
    Status queryLatestPreset(uint8_t& preset);
    Status queryExposure(PtExposureInfo& info);
    Status queryLensPositionInfo(uint16_t& zoom, uint16_t& focus, uint16_t& iris);
    Status queryLandingMode(uint8_t& mode);
    Status querySoftwareVersion(char* out, size_t out_cap);

    Status setLimitation(uint8_t mode);
    Status setLimitationControl(uint8_t pan_lim, uint8_t tilt_lim);
    Status setTiltRange(uint8_t range);
    Status setTallyEnable(uint8_t mode);
    Status setTallyEnable(bool on);
    Status setWirelessControl(uint8_t mode);
    Status setFan(uint8_t mode);
    Status setFan2(uint8_t mode);
    Status setWiper(uint8_t mode);
    Status setWasher(uint8_t mode);
    Status setOption(uint8_t mode);
    Status setPresetSpeedUnit(const char* hex3);

    Status sendCatalog(const catalog::PtCommandEntry& entry, const char* payload = nullptr);
    Status transactCatalog(
        const catalog::PtCommandEntry& entry,
        const char* payload,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

    Status sendRaw(const PtFrame& frame);
    Status transactRaw(
        const PtFrame& frame,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

    Status buildCommand(PtFrame& frame, const char* cmd, const char* payload = nullptr);
    Status sendCommand(const char* cmd, const char* payload = nullptr);
    Status transactCommand(
        const char* cmd,
        const char* payload,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

    Status sendDec1(catalog::PtCmd cmd, uint8_t value);
    Status sendSwitch(catalog::PtCmd cmd, ProtocolSwitch sw);
    Status send(catalog::PtCmd cmd, const char* payload = nullptr);
    Status transact(
        catalog::PtCmd cmd,
        const char* payload,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);
    Status sendEntry(const catalog::PtCommandEntry& entry, const char* payload = nullptr);

protected:
    Rs485Transport& transport_;

    Status sendSpeedCmd(catalog::PtCmd cmd, uint8_t speed);
    Status sendPresetCmd(catalog::PtCmd cmd, uint8_t preset);
    Status sendDec1Cmd(catalog::PtCmd cmd, uint8_t value);
    Status sendBuilt(const PtFrame& frame);
    Status transactBuilt(
        const PtFrame& frame,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms);
    Status sendHex3Cmd(const char* cmd, uint16_t value);
    Status queryHex3(const char* cmd, uint16_t& value);
};

} // namespace ccam::devices
