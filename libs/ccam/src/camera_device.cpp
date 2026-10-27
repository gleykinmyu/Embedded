#include "devices/camera_device.hpp"
#include "protocol/camera_protocol.hpp"

#include <cstring>

namespace ccam::devices {

CameraDeviceBase::CameraDeviceBase(Transport& transport, GainDbRange gain)
    : transport_(transport)
    , gain_(gain)
{
}

Status CameraDeviceBase::buildMonitor(CameraFrame& frame, const char* dxx, const char* data)
{
    if (dxx == nullptr || data == nullptr) {
        return Status::Param;
    }

    char dcmd[4];
    if (std::strlen(dxx) == 2) {
        dcmd[0] = 'D';
        dcmd[1] = dxx[0];
        dcmd[2] = dxx[1];
        dcmd[3] = '\0';
    } else if (std::strlen(dxx) == 3) {
        std::strncpy(dcmd, dxx, sizeof(dcmd));
        dcmd[3] = '\0';
    } else {
        return Status::Param;
    }

    return camBuildMonitor(frame, dcmd, data);
}

Status CameraDeviceBase::buildLensH(CameraFrame& frame, const char* hxx)
{
    if (hxx == nullptr) {
        return Status::Param;
    }

    char hcmd[4];
    if (std::strlen(hxx) == 2) {
        hcmd[0] = 'H';
        hcmd[1] = hxx[0];
        hcmd[2] = hxx[1];
        hcmd[3] = '\0';
    } else if (std::strlen(hxx) == 3) {
        std::strncpy(hcmd, hxx, sizeof(hcmd));
        hcmd[3] = '\0';
    } else {
        return Status::Param;
    }

    return camBuildContact(frame, hcmd);
}

Status CameraDeviceBase::buildMenu(
    CameraFrame& frame,
    catalog::MenuBank bank,
    uint8_t subcode,
    const char* data,
    bool set)
{
    const char* prefix = set ? catalog::menuSetPrefix(bank) : catalog::menuQueryPrefix(bank);
    if (prefix == nullptr) {
        return Status::Param;
    }

    if (set) {
        if (data == nullptr) {
            return Status::Param;
        }
        return camBuildMenuSet(frame, prefix, subcode, data);
    }

    return camBuildMenuQuery(frame, prefix, subcode);
}

void CameraDeviceBase::clearLastExchange()
{
    last_tx_len_ = 0;
    last_rx_len_ = 0;
}

void CameraDeviceBase::storeLastTx(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0) {
        last_tx_len_ = 0;
        return;
    }
    const size_t n = (len < sizeof(last_tx_)) ? len : sizeof(last_tx_);
    std::memcpy(last_tx_, data, n);
    last_tx_len_ = n;
}

void CameraDeviceBase::appendLastRx(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0) {
        return;
    }
    const size_t space = sizeof(last_rx_) - last_rx_len_;
    const size_t n = (len < space) ? len : space;
    if (n == 0) {
        return;
    }
    std::memcpy(last_rx_ + last_rx_len_, data, n);
    last_rx_len_ += n;
}

Status CameraDeviceBase::waitAck(uint32_t timeout_ms)
{
    uint8_t buf[kCameraFrameMax];
    size_t rx_len = 0;

    // Читаем до ACK / ETX / CR / timeout — чтобы видеть «голый» ответ при Nack.
    Status st = transport_.transact(nullptr, 0, buf, sizeof(buf), &rx_len, timeout_ms);
    appendLastRx(buf, rx_len);
    if (st == Status::Timeout) {
        return st;
    }
    if (st != Status::Ok || rx_len == 0) {
        return Status::Nack;
    }
    for (size_t i = 0; i < rx_len; ++i) {
        if (buf[i] == kAck) {
            return Status::Ok;
        }
    }
    return Status::Nack;
}

Status CameraDeviceBase::transact(
    const CameraFrame& frame,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    if (response_len != nullptr) {
        *response_len = 0;
    }

    clearLastExchange();
    storeLastTx(frame.bytes(), frame.size());

    Status st = transport_.send(frame.bytes(), frame.size());
    if (st != Status::Ok) {
        return st;
    }

    st = waitAck(timeout_ms);
    if (st != Status::Ok) {
        return st;
    }

    if (response == nullptr || response_cap == 0) {
        return Status::Ok;
    }

    uint8_t scratch[kCameraFrameMax];
    size_t total = 0;

    while (total < response_cap) {
        size_t chunk_len = 0;
        st = transport_.transact(nullptr, 0, scratch, sizeof(scratch), &chunk_len, timeout_ms);
        if (st != Status::Ok) {
            break;
        }

        if (chunk_len == 0) {
            break;
        }

        appendLastRx(scratch, chunk_len);

        if (total + chunk_len > response_cap) {
            return Status::Buffer;
        }

        std::memcpy(&response[total], scratch, chunk_len);
        total += chunk_len;

        if (scratch[chunk_len - 1] == kEtx) {
            break;
        }
    }

    if (response_len != nullptr) {
        *response_len = total;
    }

    return (total > 0) ? Status::Ok : Status::Timeout;
}

Status CameraDeviceBase::copyResponsePayload(
    const uint8_t* response,
    size_t response_len,
    char* out,
    size_t out_cap)
{
    if (out == nullptr || out_cap == 0) {
        return Status::Param;
    }

    if (response == nullptr || response_len < 2 || response[0] != kStx) {
        return Status::Io;
    }

    size_t copy_len = response_len;
    if (response[response_len - 1] == kEtx) {
        --copy_len;
    }
    if (response[0] == kStx) {
        --copy_len;
    }

    const size_t n = (copy_len < out_cap - 1) ? copy_len : out_cap - 1;
    std::memcpy(out, &response[1], n);
    out[n] = '\0';

    return Status::Ok;
}

Status CameraDeviceBase::sendEntry(const catalog::CameraCommandEntry& entry, const char* data)
{
    if (entry.control == nullptr || entry.control[0] == '\0') {
        return Status::Param;
    }
    if (entry.pattern != catalog::CameraPattern::Trigger && data == nullptr) {
        return Status::Param;
    }

    CameraFrame frame;
    Status st = Status::Param;

    switch (entry.pattern) {
    case catalog::CameraPattern::Trigger:
        st = camBuildTrigger(frame, entry.control);
        break;
    case catalog::CameraPattern::SetO:
    case catalog::CameraPattern::Scene:
        st = camBuildSet(frame, entry.control, data);
        break;
    case catalog::CameraPattern::Monitor:
        st = buildMonitor(frame, entry.control, data);
        break;
    default:
        return Status::Param;
    }

    if (st != Status::Ok) {
        return st;
    }

    return transact(frame);
}

Status CameraDeviceBase::requestEntry(
    const catalog::CameraCommandEntry& entry,
    char* out,
    size_t out_cap,
    uint32_t timeout_ms)
{
    if (out == nullptr || out_cap == 0 || entry.query == nullptr || entry.query[0] == '\0') {
        return Status::Param;
    }

    CameraFrame frame;
    Status st = camBuildQuery(frame, entry.query);
    if (st != Status::Ok) {
        return st;
    }

    uint8_t response[kCameraFrameMax];
    size_t response_len = 0;

    st = transact(frame, response, sizeof(response), &response_len, timeout_ms);
    if (st != Status::Ok) {
        return st;
    }

    return copyResponsePayload(response, response_len, out, out_cap);
}

Status CameraDeviceBase::send(catalog::CameraCmd cmd, const char* data)
{
    return sendEntry(catalog::getCameraCommand(cmd), data);
}

Status CameraDeviceBase::sendDec1(catalog::CameraCmd cmd, uint8_t value)
{
    char data[3];
    Status st = encodeDec1(value, data, sizeof(data));
    if (st != Status::Ok) {
        return st;
    }
    return send(cmd, data);
}

Status CameraDeviceBase::sendTrigger(catalog::CameraCmd cmd)
{
    const catalog::CameraCommandEntry& entry = catalog::getCameraCommand(cmd);
    if (entry.pattern != catalog::CameraPattern::Trigger || entry.control == nullptr ||
        entry.control[0] == '\0') {
        return Status::Param;
    }
    return triggerRaw(entry.control);
}

Status CameraDeviceBase::request(catalog::CameraCmd cmd, char* out, size_t out_cap)
{
    return requestEntry(catalog::getCameraCommand(cmd), out, out_cap);
}

Status CameraDeviceBase::triggerRaw(const char* cmd3)
{
    CameraFrame frame;
    Status st = camBuildTrigger(frame, cmd3);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::sendRaw(const char* cmd3, const char* data)
{
    CameraFrame frame;
    Status st = camBuildSet(frame, cmd3, data);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::requestRaw(const char* cmd3, char* out, size_t out_cap)
{
    CameraFrame frame;
    Status st = camBuildQuery(frame, cmd3);
    if (st != Status::Ok) {
        return st;
    }

    uint8_t response[kCameraFrameMax];
    size_t response_len = 0;

    st = transact(frame, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    return copyResponsePayload(response, response_len, out, out_cap);
}

Status CameraDeviceBase::setOsdSubcode(uint8_t subcode, const char* data)
{
    CameraFrame frame;
    Status st = camBuildOsdSet(frame, subcode, data);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::requestOsdSubcode(
    uint8_t subcode,
    char* out,
    size_t out_cap,
    uint32_t timeout_ms)
{
    CameraFrame frame;
    Status st = camBuildOsdQuery(frame, subcode);
    if (st != Status::Ok) {
        return st;
    }

    uint8_t response[kCameraFrameMax];
    size_t response_len = 0;

    st = transact(frame, response, sizeof(response), &response_len, timeout_ms);
    if (st != Status::Ok) {
        return st;
    }

    return copyResponsePayload(response, response_len, out, out_cap);
}

Status CameraDeviceBase::setMenu(catalog::MenuBank bank, uint8_t subcode, const char* data)
{
    CameraFrame frame;
    Status st = buildMenu(frame, bank, subcode, data, true);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::requestMenu(
    catalog::MenuBank bank,
    uint8_t subcode,
    char* out,
    size_t out_cap,
    uint32_t timeout_ms)
{
    CameraFrame frame;
    Status st = buildMenu(frame, bank, subcode, nullptr, false);
    if (st != Status::Ok) {
        return st;
    }

    uint8_t response[kCameraFrameMax];
    size_t response_len = 0;

    st = transact(frame, response, sizeof(response), &response_len, timeout_ms);
    if (st != Status::Ok) {
        return st;
    }

    return copyResponsePayload(response, response_len, out, out_cap);
}

Status CameraDeviceBase::selectScene(const char* data)
{
    return sendEntry(catalog::kSceneCommand, data);
}

Status CameraDeviceBase::setMonitor(const char* dxx, const char* data)
{
    CameraFrame frame;
    Status st = buildMonitor(frame, dxx, data);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::lensContact(const char* hxx)
{
    CameraFrame frame;
    Status st = buildLensH(frame, hxx);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::requestModel(char* out, size_t out_cap)
{
    return request(catalog::CameraCmd::ModelNumber, out, out_cap);
}

Status CameraDeviceBase::requestSoftwareVersion(char* out, size_t out_cap)
{
    return request(catalog::CameraCmd::SoftwareVersion, out, out_cap);
}

Status CameraDeviceBase::awcStart()
{
    return sendTrigger(catalog::CameraCmd::AwcAwbSet);
}

Status CameraDeviceBase::abcStart()
{
    return sendTrigger(catalog::CameraCmd::AbcAbbSet);
}

Status CameraDeviceBase::setAwcMode(uint8_t mode)
{
    return sendDec1(catalog::CameraCmd::AwcMode, mode);
}

Status CameraDeviceBase::setShutter(const char* data_hex)
{
    return send(catalog::CameraCmd::Shutter, data_hex);
}

namespace {

void formatHex2(uint8_t value, char out[3])
{
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(value >> 4) & 0xF];
    out[1] = hex[value & 0xF];
    out[2] = '\0';
}

void formatHex3(uint16_t value, char out[4])
{
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(value >> 8) & 0xF];
    out[1] = hex[(value >> 4) & 0xF];
    out[2] = hex[value & 0xF];
    out[3] = '\0';
}

} // namespace

Status CameraDeviceBase::setGain(int db)
{
    if (db < gain_.min_db) {
        db = gain_.min_db;
    }
    if (db > gain_.max_db) {
        db = gain_.max_db;
    }
    const uint8_t code =
        static_cast<uint8_t>(static_cast<int>(kGainZeroDbCode) + db);
    char data[3];
    formatHex2(code, data);
    return send(catalog::CameraCmd::GainUp, data);
}

Status CameraDeviceBase::setAGC()
{
    char data[3];
    formatHex2(kGainAgcCode, data);
    return send(catalog::CameraCmd::GainUp, data);
}

Status CameraDeviceBase::setDetail(uint8_t mode)
{
    return sendDec1(catalog::CameraCmd::Detail, mode);
}

Status CameraDeviceBase::setHdDetail(uint8_t mode)
{
    return sendDec1(catalog::CameraCmd::HdDetail, mode);
}

Status CameraDeviceBase::setIrisAutoManual(IrisAutoManual mode)
{
    return sendDec1(catalog::CameraCmd::IrisAutoManual, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::setManualIris(uint16_t value)
{
    const uint16_t native = normToCamIris(value);
    char padded[4];
    formatHex3(native, padded);
    return send(catalog::CameraCmd::ManualIrisVolume, padded);
}

Status CameraDeviceBase::setSceneFile(uint8_t scene)
{
    char data[4];
    Status st = encodeDecAscii(scene, data, sizeof(data));
    if (st != Status::Ok) {
        return st;
    }
    return send(catalog::CameraCmd::SceneFile, data);
}

Status CameraDeviceBase::setColorBar(Switch mode)
{
    return sendDec1(catalog::CameraCmd::ColorBar, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::setMenuOnOff(Switch mode)
{
    return sendDec1(catalog::CameraCmd::MenuOnOff, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::menuSw()
{
    return setMonitor("PG", "1"); // DPG — MENU SW
}

Status CameraDeviceBase::itemSw()
{
    return setMonitor("IT", "1"); // DIT — ITEM SW
}

Status CameraDeviceBase::yesSw()
{
    return setMonitor("UP", "1"); // DUP — YES SW
}

Status CameraDeviceBase::noSw()
{
    return setMonitor("DW", "1"); // DDW — NO SW
}

Status CameraDeviceBase::setAutoFocus(Switch mode)
{
    return sendDec1(catalog::CameraCmd::AutoFocus, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::setFilter(uint8_t mode)
{
    return sendDec1(catalog::CameraCmd::FilterNd, mode);
}

Status CameraDeviceBase::setOsd(catalog::OsdItem item, const char* data)
{
    return setOsdSubcode(catalog::subcode(item), data);
}

Status CameraDeviceBase::requestOsd(catalog::OsdItem item, char* out, size_t out_cap)
{
    return requestOsdSubcode(catalog::subcode(item), out, out_cap);
}

Status CameraDeviceBase::setVideoMenu(catalog::VideoMenuItem item, const char* data)
{
    return setMenu(catalog::MenuBank::Video, catalog::subcode(item), data);
}

Status CameraDeviceBase::requestVideoMenu(catalog::VideoMenuItem item, char* out, size_t out_cap)
{
    return requestMenu(catalog::MenuBank::Video, catalog::subcode(item), out, out_cap);
}

Status CameraDeviceBase::setExtendedJMenu(catalog::ExtendedJItem item, const char* data)
{
    return setMenu(catalog::MenuBank::ExtendedJ, catalog::subcode(item), data);
}

Status CameraDeviceBase::setExtendedJMenuSwitch(catalog::ExtendedJItem item, Switch sw)
{
    return setExtendedJMenu(item, switchPayload(sw));
}

Status CameraDeviceBase::setVideoMenuSwitch(catalog::VideoMenuItem item, Switch sw)
{
    return setVideoMenu(item, switchPayload(sw));
}

Status CameraDeviceBase::requestExtendedJMenu(catalog::ExtendedJItem item, char* out, size_t out_cap)
{
    return requestMenu(catalog::MenuBank::ExtendedJ, catalog::subcode(item), out, out_cap);
}

} // namespace ccam::devices
