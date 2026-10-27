/**
 * @file pt_device.hpp
 * @brief PtDeviceBase — Panasonic P/T Control (#…CR) поверх pt::IDevice.
 *
 * ConvertibleProtocol.pdf v3.05. Универсальный контракт: ipt_device.hpp.
 */

#pragma once

#include "catalog/catalog.hpp"
#include "devices/ipt_device.hpp"
#include "protocol/protocol_encoding.hpp"
#include "protocol/pt_types.hpp"
#include "transport/frame.hpp"
#include "transport/transport.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::devices {

/**
 * Базовое поворотное устройство: RS-422/485 и #-команды PDF.
 * Реализует pt::IDevice; Panasonic-специфика — здесь.
 */
class PtDeviceBase : public pt::IDevice {
public:
    explicit PtDeviceBase(Transport& transport);

    PtModelId modelId() const override { return PtModelId::Generic; }
    const char* modelLabel() const override { return "Generic Pan/Tilt"; }

    Status power(pt::PowerMode mode) override;
    Status move(pt::Axis axis, int8_t speed) override;
    /** #PTS — pan+tilt одной командой. */
    Status movePT(int8_t pan, int8_t tilt) override;
    Status stop() override;
    Status setPT(const pt::PT& pt) override;
    Status setLens(const pt::Lens& lens) override;
    /** Домой: центр #APC (0x8000, 0x8000). */
    Status goHome() override;
    Status recallPreset(uint8_t preset) override;
    Status savePreset(uint8_t preset) override;
    Status deletePreset(uint8_t preset) override;
    Status requestPosition(pt::Position& position) override;

    Status setPresetMode(PrMode mode);
    Status setPresetSpeed(PrSpeed speed);

    Status setPTwSpeed(
        uint16_t pan,
        uint16_t tilt,
        uint8_t preset_speed,
        PrSpeed speed_table);
    Status relativePosition(uint16_t pan, uint16_t tilt);
    Status relPTwSpeed(
        uint16_t pan,
        uint16_t tilt,
        uint8_t preset_speed,
        PrSpeed speed_table);

    /** Оптика: публично 0..FFFF → натив 555h..FFFh. */
    Status setZoom(uint16_t pos);
    Status setFocus(uint16_t pos);
    Status setIris(uint16_t pos);
    Status requestZoom(uint16_t& pos);
    Status requestFocus(uint16_t& pos);
    Status requestIris(uint16_t& pos);
    Status setLensCtrl(Switch mode);
    Status requestLensPositionInfo(uint16_t& zoom, uint16_t& focus, uint16_t& iris);

    Status requestPositionDisplay(pt::Position& position);
    Status requestError(uint8_t& error_code);
    Status requestLatestPreset(uint8_t& preset);
    Status requestExposure(pt::Exposure& exposure);
    Status requestLandingMode(LandingMode& mode);
    Status requestSoftwareVersion(char* out, size_t out_cap);

    Status setLimitation(Limitation mode);
    Status setLimitationControl(LimitAxis axis, Limitation action);
    Status setTiltRange(TiltRange range);
    Status setTallyEnable(Switch mode);
    Status setWirelessControl(Switch mode);
    Status setFan(FanMode mode);
    Status setFan2(FanMode mode);
    Status setWiper(WiperMode mode);
    Status setWasher(Switch mode);
    Status setOption(Switch mode);
    Status setPresetSpeedUnit(const char* hex3);

    /** Сырой cmd без '#'; кадр = #cmd[payload]CR (CLI / отладка). */
    Status sendCommand(const char* cmd, const char* payload = nullptr);
    Status transactCommand(
        const char* cmd,
        const char* payload,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

protected:
    Transport& transport_;

    /** signed (−128..127) → Panasonic 01..99; 0 → 50. */
    static uint8_t speedFromSigned(int8_t speed);

    Status buildCommand(PtFrame& frame, const char* cmd, const char* payload = nullptr);

    Status send(catalog::PtCmd cmd, const char* payload = nullptr);
    Status transact(
        catalog::PtCmd cmd,
        const char* payload,
        uint8_t* response,
        size_t response_cap,
        size_t* response_len,
        uint32_t timeout_ms = kDefaultRxTimeoutMs);

    Status sendDec1(catalog::PtCmd cmd, uint8_t value);
    Status sendDec2(catalog::PtCmd cmd, uint8_t value);
    Status sendHex3(catalog::PtCmd cmd, uint16_t value);
    Status queryHex3(catalog::PtCmd cmd, uint16_t& value);

    /** Сверить префикс ответа из каталога; payload_pos — после prefix. */
    static Status matchPtResponse(
        const uint8_t* data,
        size_t len,
        catalog::PtCmd cmd,
        size_t& payload_pos);
};

} // namespace ccam::devices
