#include "devices/camera_device.hpp"
#include "protocol/camera_protocol.hpp"

#include <cstring>

namespace ccam::devices {

CameraDeviceBase::CameraDeviceBase(Rs485Transport& transport)
    : transport_(transport)
{
}

Status CameraDeviceBase::buildTrigger(CameraFrame& frame, const char* cmd3)
{
    return camBuildTrigger(frame, cmd3);
}

Status CameraDeviceBase::buildSet(CameraFrame& frame, const char* cmd3, const char* data)
{
    return camBuildSet(frame, cmd3, data);
}

Status CameraDeviceBase::buildQuery(CameraFrame& frame, const char* cmd3)
{
    return camBuildQuery(frame, cmd3);
}

Status CameraDeviceBase::buildScene(CameraFrame& frame, const char* data)
{
    if (data == nullptr || data[0] == '\0') {
        return Status::Param;
    }
    return buildSet(frame, catalog::kSceneCommand.control, data);
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

Status CameraDeviceBase::buildOsdSet(CameraFrame& frame, uint8_t subcode, const char* data)
{
    return camBuildOsdSet(frame, subcode, data);
}

Status CameraDeviceBase::buildOsdQuery(CameraFrame& frame, uint8_t subcode)
{
    return camBuildOsdQuery(frame, subcode);
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

Status CameraDeviceBase::send(const CameraFrame& frame)
{
    if (frame.size() == 0) {
        return Status::Param;
    }

    return transport_.send(frame.data(), frame.size());
}

Status CameraDeviceBase::waitAck(uint32_t timeout_ms)
{
    uint8_t ack = 0;
    size_t rx_len = 0;

    Status st = transport_.transact(nullptr, 0, &ack, 1, &rx_len, timeout_ms);
    if (st == Status::Timeout) {
        return st;
    }
    if (st != Status::Ok || rx_len != 1 || ack != kAck) {
        return Status::Nack;
    }

    return Status::Ok;
}

Status CameraDeviceBase::transact(
    const CameraFrame& frame,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    if (frame.size() == 0) {
        return Status::Param;
    }

    if (response_len != nullptr) {
        *response_len = 0;
    }

    Status st = send(frame);
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

Status CameraDeviceBase::apply(const catalog::CameraCommandEntry& entry, const char* data)
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
        st = buildTrigger(frame, entry.control);
        break;
    case catalog::CameraPattern::SetO:
        st = buildSet(frame, entry.control, data);
        break;
    case catalog::CameraPattern::Scene:
        st = buildSet(frame, entry.control, data);
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

Status CameraDeviceBase::queryEntry(
    const catalog::CameraCommandEntry& entry,
    char* out,
    size_t out_cap,
    uint32_t timeout_ms)
{
    if (out == nullptr || out_cap == 0 || entry.query == nullptr || entry.query[0] == '\0') {
        return Status::Param;
    }

    CameraFrame frame;
    Status st = buildQuery(frame, entry.query);
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

Status CameraDeviceBase::setOsd(uint8_t subcode, const char* data)
{
    CameraFrame frame;
    Status st = buildOsdSet(frame, subcode, data);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::queryOsd(
    uint8_t subcode,
    char* out,
    size_t out_cap,
    uint32_t timeout_ms)
{
    CameraFrame frame;
    Status st = buildOsdQuery(frame, subcode);
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

Status CameraDeviceBase::queryMenu(
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
    return apply(catalog::kSceneCommand, data);
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

Status CameraDeviceBase::queryModel(char* out, size_t out_cap)
{
    return query(catalog::CameraCmd::ModelNumber, out, out_cap);
}

Status CameraDeviceBase::querySoftwareVersion(char* out, size_t out_cap)
{
    return query(catalog::CameraCmd::SoftwareVersion, out, out_cap);
}

Status CameraDeviceBase::awcStart()
{
    return applyTrigger(catalog::CameraCmd::AwcAwbSet);
}

Status CameraDeviceBase::abcStart()
{
    return applyTrigger(catalog::CameraCmd::AbcAbbSet);
}

Status CameraDeviceBase::setDec1Cmd(const char* cmd3, uint8_t value)
{
    char data[4];
    Status st = encodeDec1(value, data, sizeof(data));
    if (st != Status::Ok) {
        return st;
    }
    return setCmd(cmd3, data);
}

Status CameraDeviceBase::setAwcMode(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::AwcMode, mode);
}

Status CameraDeviceBase::setShutter(const char* data_hex)
{
    return apply(catalog::CameraCmd::Shutter, data_hex);
}

Status CameraDeviceBase::setGain(const char* data_hex)
{
    return apply(catalog::CameraCmd::GainUp, data_hex);
}

Status CameraDeviceBase::setDetail(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::Detail, mode);
}

Status CameraDeviceBase::setHdDetail(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::HdDetail, mode);
}

Status CameraDeviceBase::setIrisAutoManual(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::IrisAutoManual, mode);
}

Status CameraDeviceBase::setIrisAutoManual(IrisAutoManual mode)
{
    return setIrisAutoManual(static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::setManualIris(const char* data_hex)
{
    return apply(catalog::CameraCmd::ManualIrisVolume, data_hex);
}

Status CameraDeviceBase::setSceneFile(uint8_t scene)
{
    char data[4];
    Status st = encodeDecAscii(scene, data, sizeof(data));
    if (st != Status::Ok) {
        return st;
    }
    return apply(catalog::CameraCmd::SceneFile, data);
}

Status CameraDeviceBase::setColorBar(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::ColorBar, mode);
}

Status CameraDeviceBase::setMenuOnOff(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::MenuOnOff, mode);
}

Status CameraDeviceBase::setMenuOnOff(bool on)
{
    return setMenuOnOff(static_cast<uint8_t>(toProtocolSwitch(on)));
}

Status CameraDeviceBase::setAutoFocus(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::AutoFocus, mode);
}

Status CameraDeviceBase::setAutoFocus(bool on)
{
    return setAutoFocus(static_cast<uint8_t>(toProtocolSwitch(on)));
}

Status CameraDeviceBase::setFilter(uint8_t mode)
{
    return applyDec1(catalog::CameraCmd::FilterNd, mode);
}

Status CameraDeviceBase::triggerCmd(const char* cmd3)
{
    CameraFrame frame;
    Status st = buildTrigger(frame, cmd3);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::setCmd(const char* cmd3, const char* data)
{
    CameraFrame frame;
    Status st = buildSet(frame, cmd3, data);
    if (st != Status::Ok) {
        return st;
    }
    return transact(frame);
}

Status CameraDeviceBase::queryCmd(const char* cmd3, char* out, size_t out_cap)
{
    CameraFrame frame;
    Status st = buildQuery(frame, cmd3);
    if (st != Status::Ok) {
        return st;
    }

    uint8_t response[kCameraFrameMax];
    size_t response_len = 0;

    st = transact(frame, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    return copyResponsePayload(response, response_len, out, out_cap);
}

Status CameraDeviceBase::setOsd(catalog::OsdItem item, const char* data)
{
    return setOsd(catalog::subcode(item), data);
}

Status CameraDeviceBase::queryOsd(catalog::OsdItem item, char* out, size_t out_cap)
{
    return queryOsd(catalog::subcode(item), out, out_cap);
}

Status CameraDeviceBase::setVideoMenu(catalog::VideoMenuItem item, const char* data)
{
    return setMenu(catalog::MenuBank::Video, catalog::subcode(item), data);
}

Status CameraDeviceBase::queryVideoMenu(catalog::VideoMenuItem item, char* out, size_t out_cap)
{
    return queryMenu(catalog::MenuBank::Video, catalog::subcode(item), out, out_cap);
}

Status CameraDeviceBase::setExtendedJMenu(catalog::ExtendedJItem item, const char* data)
{
    return setMenu(catalog::MenuBank::ExtendedJ, catalog::subcode(item), data);
}

Status CameraDeviceBase::setExtendedJMenuSwitch(catalog::ExtendedJItem item, ProtocolSwitch sw)
{
    return setExtendedJMenu(item, protocolSwitchPayload(sw));
}

Status CameraDeviceBase::setVideoMenuSwitch(catalog::VideoMenuItem item, ProtocolSwitch sw)
{
    return setVideoMenu(item, protocolSwitchPayload(sw));
}

Status CameraDeviceBase::queryExtendedJMenu(catalog::ExtendedJItem item, char* out, size_t out_cap)
{
    return queryMenu(catalog::MenuBank::ExtendedJ, catalog::subcode(item), out, out_cap);
}

Status CameraDeviceBase::apply(catalog::CameraCmd cmd, const char* data)
{
    return apply(catalog::getCameraCommand(cmd), data);
}

Status CameraDeviceBase::applyDec1(catalog::CameraCmd cmd, uint8_t value)
{
    char data[3];
    Status st = encodeDec1(value, data, sizeof(data));
    if (st != Status::Ok) {
        return st;
    }
    return apply(cmd, data);
}

Status CameraDeviceBase::applySwitch(catalog::CameraCmd cmd, ProtocolSwitch sw)
{
    return apply(cmd, protocolSwitchPayload(sw));
}

Status CameraDeviceBase::applyTrigger(catalog::CameraCmd cmd)
{
    const catalog::CameraCommandEntry& entry = catalog::getCameraCommand(cmd);
    if (entry.pattern != catalog::CameraPattern::Trigger || entry.control == nullptr ||
        entry.control[0] == '\0') {
        return Status::Param;
    }
    return triggerCmd(entry.control);
}

Status CameraDeviceBase::query(catalog::CameraCmd cmd, char* out, size_t out_cap)
{
    return queryEntry(catalog::getCameraCommand(cmd), out, out_cap);
}

} // namespace ccam::devices
