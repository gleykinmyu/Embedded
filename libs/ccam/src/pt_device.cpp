#include "devices/pt_device.hpp"

#include <cstring>

namespace ccam::devices {

PtDeviceBase::PtDeviceBase(Rs485Transport& transport)
    : transport_(transport)
{
}

namespace {

int hexNibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

Status parseHexField(const uint8_t* data, size_t len, size_t& pos, uint16_t width, uint16_t& out)
{
    uint32_t value = 0;

    if (width == 0) {
        return Status::Param;
    }

    for (uint16_t i = 0; i < width; ++i) {
        if (pos >= len) {
            return Status::Io;
        }

        const int nibble = hexNibble(static_cast<char>(data[pos]));
        if (nibble < 0) {
            return Status::Io;
        }

        value = (value << 4) | static_cast<uint32_t>(nibble);
        ++pos;
    }

    out = static_cast<uint16_t>(value);
    return Status::Ok;
}

bool isSpeedValid(uint8_t speed)
{
    return speed >= kPtSpeedMin && speed <= kPtSpeedMax;
}

} // namespace

uint8_t PtDeviceBase::speedForDirection(PtAxisDir dir, uint8_t rate_1_to_49)
{
    uint8_t rate = rate_1_to_49;
    if (rate < kPtSpeedMin) {
        rate = kPtSpeedMin;
    }
    if (rate > 49) {
        rate = 49;
    }

    switch (dir) {
    case PtAxisDir::Left:
    case PtAxisDir::Down:
    case PtAxisDir::Wide:
    case PtAxisDir::Near:
        return rate;

    case PtAxisDir::Right:
    case PtAxisDir::Up:
    case PtAxisDir::Tele:
    case PtAxisDir::Far:
        return static_cast<uint8_t>(kPtSpeedMax - rate + 1);

    default:
        return kPtSpeedStop;
    }
}

Status PtDeviceBase::buildCommand(PtFrame& frame, const char* cmd, const char* payload)
{
    if (cmd == nullptr) {
        return Status::Param;
    }

    frame.clear();

    Status st = frame.appendByte(kPtPrefix);
    if (st != Status::Ok) {
        return st;
    }

    st = frame.appendStr(cmd);
    if (st != Status::Ok) {
        return st;
    }

    if (payload != nullptr) {
        st = frame.appendStr(payload);
        if (st != Status::Ok) {
            return st;
        }
    }

    return frame.appendByte(kPtCr);
}

Status PtDeviceBase::sendBuilt(const PtFrame& frame)
{
    if (frame.size() == 0) {
        return Status::Param;
    }
    return transport_.send(frame.data(), frame.size());
}

Status PtDeviceBase::transactBuilt(
    const PtFrame& frame,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    if (frame.size() == 0) {
        return Status::Param;
    }

    return transport_.transact(
        frame.data(),
        frame.size(),
        response,
        response_cap,
        response_len,
        timeout_ms);
}

Status PtDeviceBase::sendRaw(const PtFrame& frame)
{
    return sendBuilt(frame);
}

Status PtDeviceBase::transactRaw(
    const PtFrame& frame,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    return transactBuilt(frame, response, response_cap, response_len, timeout_ms);
}

Status PtDeviceBase::sendCommand(const char* cmd, const char* payload)
{
    PtFrame frame;
    Status st = buildCommand(frame, cmd, payload);
    if (st != Status::Ok) {
        return st;
    }
    return sendBuilt(frame);
}

Status PtDeviceBase::transactCommand(
    const char* cmd,
    const char* payload,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    PtFrame frame;
    Status st = buildCommand(frame, cmd, payload);
    if (st != Status::Ok) {
        return st;
    }
    return transactBuilt(frame, response, response_cap, response_len, timeout_ms);
}

Status PtDeviceBase::sendCatalog(const catalog::PtCommandEntry& entry, const char* payload)
{
    if (entry.control == nullptr || entry.control[0] == '\0') {
        return Status::Param;
    }

    const char* cmd = entry.control;
    if (cmd[0] == '#') {
        ++cmd;
    }

    return sendCommand(cmd, payload);
}

Status PtDeviceBase::transactCatalog(
    const catalog::PtCommandEntry& entry,
    const char* payload,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    if (entry.control == nullptr || entry.control[0] == '\0') {
        return Status::Param;
    }

    const char* cmd = entry.control;
    if (cmd[0] == '#') {
        ++cmd;
    }

    return transactCommand(cmd, payload, response, response_cap, response_len, timeout_ms);
}

Status PtDeviceBase::sendSpeedCmd(catalog::PtCmd cmd, uint8_t speed)
{
    if (!isSpeedValid(speed)) {
        return Status::Param;
    }

    char payload[3];
    payload[0] = static_cast<char>('0' + (speed / 10));
    payload[1] = static_cast<char>('0' + (speed % 10));
    payload[2] = '\0';

    return send(cmd, payload);
}

Status PtDeviceBase::setPanSpeed(uint8_t speed)
{
    return sendSpeedCmd(catalog::PtCmd::PanSpeed, speed);
}
Status PtDeviceBase::setTiltSpeed(uint8_t speed)
{
    return sendSpeedCmd(catalog::PtCmd::TiltSpeed, speed);
}
Status PtDeviceBase::setZoomSpeed(uint8_t speed)
{
    return sendSpeedCmd(catalog::PtCmd::ZoomSpeed, speed);
}
Status PtDeviceBase::setFocusSpeed(uint8_t speed)
{
    return sendSpeedCmd(catalog::PtCmd::FocusSpeed, speed);
}
Status PtDeviceBase::setRollSpeed(uint8_t speed)
{
    return sendSpeedCmd(catalog::PtCmd::RollSpeed, speed);
}
Status PtDeviceBase::setIrisSpeed(uint8_t speed)
{
    return sendSpeedCmd(catalog::PtCmd::IrisSpeed, speed);
}

Status PtDeviceBase::panStop() { return setPanSpeed(kPtSpeedStop); }
Status PtDeviceBase::tiltStop() { return setTiltSpeed(kPtSpeedStop); }
Status PtDeviceBase::zoomStop() { return setZoomSpeed(kPtSpeedStop); }
Status PtDeviceBase::focusStop() { return setFocusSpeed(kPtSpeedStop); }
Status PtDeviceBase::rollStop() { return setRollSpeed(kPtSpeedStop); }
Status PtDeviceBase::irisStop() { return setIrisSpeed(kPtSpeedStop); }

Status PtDeviceBase::stopAll()
{
    Status st = panStop();
    if (st != Status::Ok) {
        return st;
    }
    st = tiltStop();
    if (st != Status::Ok) {
        return st;
    }
    st = zoomStop();
    if (st != Status::Ok) {
        return st;
    }
    st = focusStop();
    if (st != Status::Ok) {
        return st;
    }
    st = rollStop();
    if (st != Status::Ok) {
        return st;
    }
    return irisStop();
}

Status PtDeviceBase::movePan(PtAxisDir dir, uint8_t rate_1_to_49)
{
    if (dir != PtAxisDir::Left && dir != PtAxisDir::Right) {
        return Status::Param;
    }
    return setPanSpeed(speedForDirection(dir, rate_1_to_49));
}

Status PtDeviceBase::moveTilt(PtAxisDir dir, uint8_t rate_1_to_49)
{
    if (dir != PtAxisDir::Down && dir != PtAxisDir::Up) {
        return Status::Param;
    }
    return setTiltSpeed(speedForDirection(dir, rate_1_to_49));
}

Status PtDeviceBase::moveZoom(PtAxisDir dir, uint8_t rate_1_to_49)
{
    if (dir != PtAxisDir::Wide && dir != PtAxisDir::Tele) {
        return Status::Param;
    }
    return setZoomSpeed(speedForDirection(dir, rate_1_to_49));
}

Status PtDeviceBase::moveFocus(PtAxisDir dir, uint8_t rate_1_to_49)
{
    if (dir != PtAxisDir::Near && dir != PtAxisDir::Far) {
        return Status::Param;
    }
    return setFocusSpeed(speedForDirection(dir, rate_1_to_49));
}

Status PtDeviceBase::moveRoll(PtAxisDir dir, uint8_t rate_1_to_49)
{
    if (dir != PtAxisDir::Left && dir != PtAxisDir::Right) {
        return Status::Param;
    }
    return setRollSpeed(speedForDirection(dir, rate_1_to_49));
}

Status PtDeviceBase::moveIris(PtAxisDir dir, uint8_t rate_1_to_49)
{
    if (dir != PtAxisDir::Near && dir != PtAxisDir::Far) {
        return Status::Param;
    }
    return setIrisSpeed(speedForDirection(dir, rate_1_to_49));
}

Status PtDeviceBase::setPanTiltSpeed(uint8_t pan_speed, uint8_t tilt_speed)
{
    if (pan_speed < kPtSpeedMin || pan_speed > kPtSpeedMax ||
        tilt_speed < kPtSpeedMin || tilt_speed > kPtSpeedMax) {
        return Status::Param;
    }

    char payload[5];
    payload[0] = static_cast<char>('0' + (pan_speed / 10));
    payload[1] = static_cast<char>('0' + (pan_speed % 10));
    payload[2] = static_cast<char>('0' + (tilt_speed / 10));
    payload[3] = static_cast<char>('0' + (tilt_speed % 10));
    payload[4] = '\0';

    return send(catalog::PtCmd::PanTiltSpeed, payload);
}

Status PtDeviceBase::power(PtPowerMode mode)
{
    uint8_t value = 0;

    switch (mode) {
    case PtPowerMode::Off:
        value = 0;
        break;
    case PtPowerMode::On:
        value = 1;
        break;
    case PtPowerMode::OnWithCameraTx:
        value = 2;
        break;
    case PtPowerMode::OnWithoutCameraTx:
        value = 3;
        break;
    default:
        return Status::Param;
    }

    return sendDec1(catalog::PtCmd::Power, value);
}

Status PtDeviceBase::sendPresetCmd(catalog::PtCmd cmd, uint8_t preset)
{
    if (preset > 99) {
        return Status::Param;
    }

    char payload[3];
    payload[0] = static_cast<char>('0' + (preset / 10));
    payload[1] = static_cast<char>('0' + (preset % 10));
    payload[2] = '\0';

    return send(cmd, payload);
}

Status PtDeviceBase::sendDec1Cmd(catalog::PtCmd cmd, uint8_t value)
{
    return sendDec1(cmd, value);
}

Status PtDeviceBase::sendHex3Cmd(const char* cmd, uint16_t value)
{
    if (value > 0xFFF) {
        return Status::Param;
    }

    PtFrame frame;
    frame.clear();

    Status st = frame.appendByte(kPtPrefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(cmd);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(value, 3);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(kPtCr);
    if (st != Status::Ok) {
        return st;
    }

    return sendBuilt(frame);
}

Status PtDeviceBase::queryHex3(const char* cmd, uint16_t& value)
{
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transactCommand(cmd, nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 6) {
        return Status::Io;
    }

    size_t pos = 3;
    return parseHexField(response, response_len, pos, 3, value);
}

Status PtDeviceBase::recallPreset(uint8_t preset)
{
    return sendPresetCmd(catalog::PtCmd::RecallPreset, preset);
}
Status PtDeviceBase::savePreset(uint8_t preset)
{
    return sendPresetCmd(catalog::PtCmd::SavePreset, preset);
}
Status PtDeviceBase::deletePreset(uint8_t preset)
{
    return sendPresetCmd(catalog::PtCmd::DeletePreset, preset);
}
Status PtDeviceBase::setPresetMode(uint8_t mode) { return sendDec1Cmd("RT", mode); }
Status PtDeviceBase::setPresetSpeedTable(uint8_t table) { return sendDec1Cmd("PST", table); }

Status PtDeviceBase::absolutePosition(uint16_t pan, uint16_t tilt)
{
    PtFrame frame;
    frame.clear();

    Status st = frame.appendByte(kPtPrefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(catalog::ptControl(catalog::PtCmd::PanTiltAbsolute));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(kPtCr);
    if (st != Status::Ok) {
        return st;
    }

    return sendBuilt(frame);
}

Status PtDeviceBase::absolutePositionWithSpeed(
    uint16_t pan,
    uint16_t tilt,
    uint8_t preset_speed,
    uint8_t speed_table)
{
    PtFrame frame;
    frame.clear();

    Status st = frame.appendByte(kPtPrefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(catalog::ptControl(catalog::PtCmd::PanTiltAbsoluteSpeed));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(preset_speed, 2);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(speed_table, 1);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(kPtCr);
    if (st != Status::Ok) {
        return st;
    }

    return sendBuilt(frame);
}

Status PtDeviceBase::relativePosition(uint16_t pan, uint16_t tilt)
{
    PtFrame frame;
    frame.clear();

    Status st = frame.appendByte(kPtPrefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(catalog::ptControl(catalog::PtCmd::PanTiltRelative));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(kPtCr);
    if (st != Status::Ok) {
        return st;
    }

    return sendBuilt(frame);
}

Status PtDeviceBase::relativePositionWithSpeed(
    uint16_t pan,
    uint16_t tilt,
    uint8_t preset_speed,
    uint8_t speed_table)
{
    PtFrame frame;
    frame.clear();

    Status st = frame.appendByte(kPtPrefix);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendStr(catalog::ptControl(catalog::PtCmd::PanTiltRelativeSpeed));
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(preset_speed, 2);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendHex(speed_table, 1);
    if (st != Status::Ok) {
        return st;
    }
    st = frame.appendByte(kPtCr);
    if (st != Status::Ok) {
        return st;
    }

    return sendBuilt(frame);
}

Status PtDeviceBase::sendCatalogDec1(catalog::PtCmd cmd, uint8_t value)
{
    char payload[3];
    Status st = encodeDec1(value, payload, sizeof(payload));
    if (st != Status::Ok) {
        return st;
    }
    return sendCatalog(catalog::getPtCommand(cmd), payload);
}

Status PtDeviceBase::sendCatalogSwitch(catalog::PtCmd cmd, ProtocolSwitch sw)
{
    return sendCatalog(catalog::getPtCommand(cmd), protocolSwitchPayload(sw));
}

Status PtDeviceBase::setZoomPosition(uint16_t pos)
{
    return sendHex3Cmd(catalog::ptControl(catalog::PtCmd::ZoomPositionSet), pos);
}
Status PtDeviceBase::setFocusPosition(uint16_t pos)
{
    return sendHex3Cmd(catalog::ptControl(catalog::PtCmd::FocusPositionSet), pos);
}
Status PtDeviceBase::setIrisPosition(uint16_t pos)
{
    return sendHex3Cmd(catalog::ptControl(catalog::PtCmd::IrisPositionSet), pos);
}
Status PtDeviceBase::queryZoomPosition(uint16_t& pos)
{
    return queryHex3(catalog::ptControl(catalog::PtCmd::ZoomPositionQuery), pos);
}
Status PtDeviceBase::queryFocusPosition(uint16_t& pos)
{
    return queryHex3(catalog::ptControl(catalog::PtCmd::FocusPositionQuery), pos);
}
Status PtDeviceBase::queryIrisPosition(uint16_t& pos)
{
    return queryHex3(catalog::ptControl(catalog::PtCmd::IrisPositionQuery), pos);
}
Status PtDeviceBase::setLensPositionCtrl(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::LensPositionCtrl, mode);
}

Status PtDeviceBase::queryPosition(PtPosition& position)
{
    PtFrame frame;
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;
    size_t pos = 0;

    Status st = buildCommand(frame, catalog::ptControl(catalog::PtCmd::GetPtzPosRaw));
    if (st != Status::Ok) {
        return st;
    }

    st = transactBuilt(frame, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 4 || response[0] != 'p' || response[1] != 'T' || response[2] != 'V') {
        return Status::Io;
    }

    pos = 3;
    st = parseHexField(response, response_len, pos, 4, position.pan);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 4, position.tilt);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, position.zoom);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, position.focus);
    if (st != Status::Ok) {
        return st;
    }

    return parseHexField(response, response_len, pos, 3, position.iris);
}

Status PtDeviceBase::queryError(uint8_t& error_code)
{
    PtFrame frame;
    uint8_t response[16];
    size_t response_len = 0;

    Status st = buildCommand(frame, "RER");
    if (st != Status::Ok) {
        return st;
    }

    st = transactBuilt(frame, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 4 || response[0] != 'r' || response[1] != 'E' || response[2] != 'R') {
        return Status::Io;
    }

    const int hi = hexNibble(static_cast<char>(response[3]));
    const int lo = (response_len > 4) ? hexNibble(static_cast<char>(response[4])) : -1;

    if (hi < 0) {
        return Status::Io;
    }

    error_code = static_cast<uint8_t>((lo >= 0) ? ((hi << 4) | lo) : hi);
    return Status::Ok;
}

Status PtDeviceBase::queryPositionDisplay(PtPosition& position)
{
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;
    size_t pos = 0;

    Status st = transactCommand("PTD", nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 4 || response[0] != 'p' || response[1] != 'T' || response[2] != 'D') {
        return Status::Io;
    }

    pos = 3;
    st = parseHexField(response, response_len, pos, 4, position.pan);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 4, position.tilt);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, position.zoom);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, position.focus);
    if (st != Status::Ok) {
        return st;
    }

    return parseHexField(response, response_len, pos, 3, position.iris);
}

Status PtDeviceBase::queryLatestPreset(uint8_t& preset)
{
    uint8_t response[8];
    size_t response_len = 0;

    Status st = transactCommand("S", nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 3 || response[0] != 's') {
        return Status::Io;
    }

    const int tens = (response_len > 1 && response[1] >= '0' && response[1] <= '9') ? (response[1] - '0') : 0;
    const int ones = (response_len > 2 && response[2] >= '0' && response[2] <= '9') ? (response[2] - '0') : 0;
    preset = static_cast<uint8_t>(tens * 10 + ones);
    return Status::Ok;
}

Status PtDeviceBase::queryExposure(PtExposureInfo& info)
{
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;
    size_t pos = 0;

    Status st = transactCommand("PTG", nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 4 || response[0] != 'p' || response[1] != 'T' || response[2] != 'G') {
        return Status::Io;
    }

    pos = 3;
    st = parseHexField(response, response_len, pos, 3, info.gain);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, info.shutter);
    if (st != Status::Ok) {
        return st;
    }

    return parseHexField(response, response_len, pos, 3, info.nd);
}

Status PtDeviceBase::queryLensPositionInfo(uint16_t& zoom, uint16_t& focus, uint16_t& iris)
{
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;
    size_t pos = 0;

    Status st = transactCommand("LPI", nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 4 || response[0] != 'l' || response[1] != 'P' || response[2] != 'I') {
        return Status::Io;
    }

    pos = 3;
    st = parseHexField(response, response_len, pos, 3, zoom);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, focus);
    if (st != Status::Ok) {
        return st;
    }

    return parseHexField(response, response_len, pos, 3, iris);
}

Status PtDeviceBase::queryLandingMode(uint8_t& mode)
{
    uint8_t response[8];
    size_t response_len = 0;

    Status st = transactCommand("N", nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 2 || response[0] != 'n') {
        return Status::Io;
    }

    mode = static_cast<uint8_t>(response[1] - '0');
    return Status::Ok;
}

Status PtDeviceBase::querySoftwareVersion(char* out, size_t out_cap)
{
    if (out == nullptr || out_cap == 0) {
        return Status::Param;
    }

    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transactCommand("QSV", nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    if (response_len < 4) {
        return Status::Io;
    }

    size_t copy_len = response_len;
    if (response[response_len - 1] == kPtCr) {
        --copy_len;
    }

    const size_t skip = 3;
    if (copy_len <= skip) {
        out[0] = '\0';
        return Status::Ok;
    }

    copy_len -= skip;
    const size_t n = (copy_len < out_cap - 1) ? copy_len : out_cap - 1;
    std::memcpy(out, &response[skip], n);
    out[n] = '\0';
    return Status::Ok;
}

Status PtDeviceBase::setLimitation(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::LimitationSet, mode);
}

Status PtDeviceBase::setLimitationControl(uint8_t pan_lim, uint8_t tilt_lim)
{
    if (pan_lim > 9 || tilt_lim > 9) {
        return Status::Param;
    }

    char payload[3];
    payload[0] = static_cast<char>('0' + pan_lim);
    payload[1] = static_cast<char>('0' + tilt_lim);
    payload[2] = '\0';

    return send(catalog::PtCmd::LimitationControl, payload);
}

Status PtDeviceBase::setTiltRange(uint8_t range)
{
    return sendDec1(catalog::PtCmd::TiltRange, range);
}
Status PtDeviceBase::setTallyEnable(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::TallyEnable, mode);
}

Status PtDeviceBase::setTallyEnable(bool on)
{
    return setTallyEnable(static_cast<uint8_t>(toProtocolSwitch(on)));
}
Status PtDeviceBase::setWirelessControl(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::WirelessControl, mode);
}
Status PtDeviceBase::setFan(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::Fan, mode);
}
Status PtDeviceBase::setFan2(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::Fan2, mode);
}
Status PtDeviceBase::setWiper(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::WiperCtrl, mode);
}
Status PtDeviceBase::setWasher(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::Washer, mode);
}
Status PtDeviceBase::setOption(uint8_t mode)
{
    return sendDec1(catalog::PtCmd::Option, mode);
}
Status PtDeviceBase::setPresetSpeedUnit(const char* hex3)
{
    if (hex3 == nullptr || std::strlen(hex3) != 3) {
        return Status::Param;
    }
    return send(catalog::PtCmd::PresetSpeedUnit, hex3);
}

Status PtDeviceBase::sendDec1(catalog::PtCmd cmd, uint8_t value)
{
    return sendCatalogDec1(cmd, value);
}

Status PtDeviceBase::sendSwitch(catalog::PtCmd cmd, ProtocolSwitch sw)
{
    return sendCatalogSwitch(cmd, sw);
}

Status PtDeviceBase::send(catalog::PtCmd cmd, const char* payload)
{
    return sendCatalog(catalog::getPtCommand(cmd), payload);
}

Status PtDeviceBase::transact(
    catalog::PtCmd cmd,
    const char* payload,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    return transactCatalog(
        catalog::getPtCommand(cmd), payload, response, response_cap, response_len, timeout_ms);
}

Status PtDeviceBase::sendEntry(const catalog::PtCommandEntry& entry, const char* payload)
{
    return sendCatalog(entry, payload);
}

} // namespace ccam::devices
