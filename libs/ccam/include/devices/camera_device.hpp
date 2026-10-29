/**
 * @file camera_device.hpp
 * @brief Камера Convertible Protocol — протокол + модель в одном объекте.
 *
 * Кадр: [STX][команда][:data][ETX]. ConvertibleProtocol.pdf v3.05, Camera Control.
 * Pan/tilt/zoom — через PtDeviceBase (#P, #T, #Z, #F).
 */

#pragma once

#include "catalog/catalog.hpp"
#include "devices/camera_limits.hpp"
#include "devices/device_caps.hpp"
#include "devices/device_types.hpp"
#include "protocol/protocol_encoding.hpp"
#include "transport/frame.hpp"
#include "transport/transport.hpp"
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

    virtual Status requestModel(char* out, size_t out_cap) = 0;
    virtual Status awcStart() = 0;
    virtual Status setAwcMode(uint8_t mode) = 0;
};

/**
 * Базовая камера: транспорт и O/Q/OSD/меню.
 * Диапазон gain (дБ) задаётся в конструкторе конкретной модели.
 */
class CameraDeviceBase : public ICameraDevice {
public:
    /** Натив ORV (после map из публичных 0..FFFF). */
    static constexpr uint16_t kIrisMin = kCamIrisNativeMin;
    static constexpr uint16_t kIrisMax = kCamIrisNativeMax;

    /** OGU: 0 dB ↔ 08h; AGC ON ↔ 80h. */
    static constexpr uint8_t kGainZeroDbCode = 0x08;
    static constexpr uint8_t kGainAgcCode = 0x80;

    explicit CameraDeviceBase(Transport& transport, GainDbRange gain = kGainDbStudio);

    CameraModelId modelId() const override { return CameraModelId::Generic; }
    const char* modelLabel() const override { return "Generic Convertible Camera"; }
    virtual CamCaps capabilities() const { return camCapsFor(modelId()); }

    Status requestModel(char* out, size_t out_cap) override;
    Status requestSoftwareVersion(char* out, size_t out_cap);

    Status awcStart() override;
    Status abcStart();
    /** Alias ABC/ABB (OAS) — удобный для UI. */
    Status abcBalanceStart() { return abcStart(); }
    Status setAwcMode(uint8_t mode) override;

    Status setShutter(const char* data_hex);
    /** OSH — одна hex-цифра 0..F. */
    Status setShutterMode(uint8_t mode_0_to_f);
    Status setSynchroScan(const char* data);
    /** OGU в дБ → code = 0x08 + dB, clamp к gainRange(). */
    Status setGain(int db);
    /** OGU:80h — AGC ON. */
    Status setAGC();
    Status setDetail(uint8_t mode);
    Status setHdDetail(uint8_t mode);
    Status setIrisAutoManual(IrisAutoManual mode);
    /** ORV: публично 0..FFFF → натив 000h..3FFh. */
    Status setManualIris(uint16_t value);
    Status setSceneFile(uint8_t scene);
    Status setColorBar(Switch mode);
    /** OUS — Menu display On/Off. */
    Status setMenuOnOff(Switch mode);
    /** DPG:1 — MENU SW / Up (RCU). */
    Status menuSw();
    /** DIT:1 — ITEM SW / Down (RCU). */
    Status itemSw();
    /** DUP:1 — YES SW / «+». */
    Status yesSw();
    /** DDW:1 — NO SW / «−». */
    Status noSw();
    Status setAutoFocus(Switch mode);
    Status setFilter(uint8_t mode);

    const GainDbRange& gainRange() const { return gain_; }

    Status selectScene(const char* data);
    Status setMonitor(const char* dxx, const char* data);
    Status lensContact(const char* hxx);

    Status setNegPos(bool negative);
    Status setFieldFrame(FieldFrameMode mode);
    Status setChromaLevel(Levels level);
    Status setRGain(const char* data);
    Status setBGain(const char* data);
    Status setRPedestal(const char* data);
    Status setBPedestal(const char* data);
    Status setGamma(const char* data);
    Status setAgcMax(const char* data);
    Status setTotalDtlLevel(const char* data);
    Status setGenlockInput(const char* data);
    Status setBlackShadingCorrect(bool on);

    Status setOsd(catalog::OsdItem item, const char* data);
    Status requestOsd(catalog::OsdItem item, char* out, size_t out_cap);
    Status setVideoMenu(catalog::VideoMenuItem item, const char* data);
    Status requestVideoMenu(catalog::VideoMenuItem item, char* out, size_t out_cap);
    Status setExtendedJMenu(catalog::ExtendedJItem item, const char* data);
    Status setExtendedJMenuSwitch(catalog::ExtendedJItem item, Switch sw);
    Status setVideoMenuSwitch(catalog::VideoMenuItem item, Switch sw);
    Status requestExtendedJMenu(catalog::ExtendedJItem item, char* out, size_t out_cap);

    /** Типизированные запросы (парсят data после ':'). */
    Status requestAwcMode(uint8_t& mode);
    /** OGU: agc=true при 80h, иначе db = code − 08h. */
    Status requestGain(int& db, bool& agc);
    Status requestShutter(uint8_t& code);
    Status requestIrisAutoManual(IrisAutoManual& mode);
    /** ORV → публичная шкала 0..FFFF. */
    Status requestManualIris(uint16_t& norm);
    Status requestDetail(uint8_t& mode);
    Status requestSceneFile(uint8_t& scene);
    Status requestColorBar(Switch& mode);
    Status requestMenuOnOff(Switch& mode);
    Status requestAutoFocus(Switch& mode);
    Status requestFilter(uint8_t& mode);
    Status requestNegPos(Switch& mode);
    Status requestFieldFrame(FieldFrameMode& mode);
    Status requestChromaLevel(uint8_t& level);
    Status requestRGain(uint16_t& value);
    Status requestBGain(uint16_t& value);

    const uint8_t* lastTx() const { return last_tx_; }
    size_t lastTxLen() const { return last_tx_len_; }
    const uint8_t* lastRx() const { return last_rx_; }
    size_t lastRxLen() const { return last_rx_len_; }

protected:
    Transport& transport_;
    GainDbRange gain_;

    /** Query Qxx → сырой payload без STX/ETX (напр. "OAW:1"). */
    Status request(catalog::CameraCmd cmd, char* out, size_t out_cap);

    Status send(catalog::CameraCmd cmd, const char* data = nullptr);
    Status sendDec1(catalog::CameraCmd cmd, uint8_t value);
    Status sendTrigger(catalog::CameraCmd cmd);

    Status sendRaw(const char* cmd3, const char* data = nullptr);
    Status triggerRaw(const char* cmd3);
    Status requestRaw(const char* cmd3, char* out, size_t out_cap);

    Status setMenu(catalog::MenuBank bank, uint8_t subcode, const char* data);
    Status requestMenu(
        catalog::MenuBank bank,
        uint8_t subcode,
        char* out,
        size_t out_cap,
        uint32_t timeout_ms = kCameraRxTimeoutMs);

    Status transact(
        const CameraFrame& frame,
        uint8_t* response = nullptr,
        size_t response_cap = 0,
        size_t* response_len = nullptr,
        uint32_t timeout_ms = kCameraRxTimeoutMs);

    Status waitAck(uint32_t timeout_ms);
    /** Слить запоздавшие STX…ETX после ACK (set без query-ответа). */
    void drainTrailingFrames(uint32_t settle_ms);
    Status copyResponsePayload(
        const uint8_t* response,
        size_t response_len,
        char* out,
        size_t out_cap);

private:
    Status requestDecU8(catalog::CameraCmd cmd, uint8_t& value);
    Status requestSwitchField(catalog::CameraCmd cmd, Switch& mode);

    Status sendEntry(const catalog::CameraCommandEntry& entry, const char* data);
    Status requestEntry(
        const catalog::CameraCommandEntry& entry,
        char* out,
        size_t out_cap,
        uint32_t timeout_ms = kCameraRxTimeoutMs);

    Status setOsdSubcode(uint8_t subcode, const char* data);
    Status requestOsdSubcode(
        uint8_t subcode,
        char* out,
        size_t out_cap,
        uint32_t timeout_ms = kCameraRxTimeoutMs);

    Status buildMonitor(CameraFrame& frame, const char* dxx, const char* data);
    Status buildLensH(CameraFrame& frame, const char* hxx);
    Status buildMenu(
        CameraFrame& frame,
        catalog::MenuBank bank,
        uint8_t subcode,
        const char* data,
        bool set);

    void clearLastExchange();
    void storeLastTx(const uint8_t* data, size_t len);
    void appendLastRx(const uint8_t* data, size_t len);

    uint8_t last_tx_[kCameraFrameMax]{};
    size_t last_tx_len_{0};
    uint8_t last_rx_[kCameraFrameMax]{};
    size_t last_rx_len_{0};
};

} // namespace ccam::devices
