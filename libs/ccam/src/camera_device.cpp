#include "devices/camera_device.hpp"
#include "protocol/camera_protocol.hpp"
#include "protocol/choice_lists.hpp"

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
    // Камера/мост может отдать запоздавший STX…ETX от прошлого Q/O.
    // Нельзя принимать первый ETX как Nack — ждём ACK до дедлайна.
    const uint32_t t0 = transport_.nowMs();
    for (;;) {
        const uint32_t elapsed = transport_.nowMs() - t0;
        if (elapsed >= timeout_ms) {
            transport_.purge();
            return Status::Timeout;
        }
        const uint32_t left = timeout_ms - elapsed;

        uint8_t buf[kCameraFrameMax];
        size_t rx_len = 0;
        Status st = transport_.transact(nullptr, 0, buf, sizeof(buf), &rx_len, left);
        appendLastRx(buf, rx_len);

        if (st == Status::Timeout) {
            transport_.purge();
            return Status::Timeout;
        }
        if (st != Status::Ok || rx_len == 0) {
            continue;
        }

        for (size_t i = 0; i < rx_len; ++i) {
            if (buf[i] == kAck) {
                return Status::Ok;
            }
        }
        // STX…ETX / мусор без ACK — отбрасываем и продолжаем ждать.
    }
}

void CameraDeviceBase::drainTrailingFrames(uint32_t settle_ms)
{
    // После ACK камера иногда шлёт ещё <STX>Oxx:…<ETX> — иначе портит следующий set.
    const uint32_t t0 = transport_.nowMs();
    uint32_t last_rx_ms = t0;
    while (transport_.nowMs() - last_rx_ms < settle_ms) {
        if (transport_.nowMs() - t0 >= settle_ms * 4u) {
            break;
        }
        uint8_t buf[kCameraFrameMax];
        size_t n = 0;
        const Status st =
            transport_.transact(nullptr, 0, buf, sizeof(buf), &n, settle_ms);
        if (st != Status::Ok || n == 0) {
            break;
        }
        appendLastRx(buf, n);
        last_rx_ms = transport_.nowMs();
        bool only_ack = true;
        for (size_t i = 0; i < n; ++i) {
            if (buf[i] != kAck) {
                only_ack = false;
                break;
            }
        }
        if (only_ack) {
            break;
        }
    }
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
        drainTrailingFrames(60);
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

namespace {

const char* dataAfterColon(const char* payload)
{
    if (payload == nullptr) {
        return nullptr;
    }
    const char* p = payload;
    while (*p != '\0' && *p != kCmdSep) {
        ++p;
    }
    if (*p != kCmdSep || p[1] == '\0') {
        return nullptr;
    }
    return p + 1;
}

int hexNibbleCam(char c)
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

Status parseDecU8(const char* data, uint8_t& out)
{
    if (data == nullptr || data[0] == '\0') {
        return Status::Io;
    }
    unsigned v = 0;
    for (const char* p = data; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return Status::Io;
        }
        v = v * 10u + static_cast<unsigned>(*p - '0');
        if (v > 255u) {
            return Status::Io;
        }
    }
    out = static_cast<uint8_t>(v);
    return Status::Ok;
}

Status parseHexU16(const char* data, uint16_t& out)
{
    if (data == nullptr || data[0] == '\0') {
        return Status::Io;
    }
    uint32_t v = 0;
    size_t n = 0;
    for (const char* p = data; *p; ++p, ++n) {
        const int nib = hexNibbleCam(*p);
        if (nib < 0 || n >= 4) {
            return Status::Io;
        }
        v = (v << 4) | static_cast<uint32_t>(nib);
    }
    out = static_cast<uint16_t>(v);
    return Status::Ok;
}

Status copyDataField(const char* payload, char* out, size_t out_cap)
{
    if (out == nullptr || out_cap == 0) {
        return Status::Param;
    }
    const char* data = dataAfterColon(payload);
    if (data == nullptr) {
        data = payload;
    }
    if (data == nullptr) {
        out[0] = '\0';
        return Status::Io;
    }
    size_t i = 0;
    for (; data[i] != '\0' && i + 1 < out_cap; ++i) {
        out[i] = data[i];
    }
    out[i] = '\0';
    return Status::Ok;
}

} // namespace

Status CameraDeviceBase::requestDecU8(catalog::CameraCmd cmd, uint8_t& value)
{
    char raw[40] = {};
    Status st = request(cmd, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    return parseDecU8(data, value);
}

Status CameraDeviceBase::requestSwitchField(catalog::CameraCmd cmd, Switch& mode)
{
    uint8_t v = 0;
    Status st = requestDecU8(cmd, v);
    if (st != Status::Ok) {
        return st;
    }
    if (v > 1) {
        return Status::Io;
    }
    mode = static_cast<Switch>(v);
    return Status::Ok;
}

Status CameraDeviceBase::requestModel(char* out, size_t out_cap)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::ModelNumber, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    return copyDataField(raw, out, out_cap);
}

Status CameraDeviceBase::requestSoftwareVersion(char* out, size_t out_cap)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::SoftwareVersion, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    return copyDataField(raw, out, out_cap);
}

Status CameraDeviceBase::requestAwcMode(uint8_t& mode)
{
    return requestDecU8(catalog::CameraCmd::AwcMode, mode);
}

Status CameraDeviceBase::requestGain(int& db, bool& agc)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::GainUp, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    // Только hex-цифры (отрезать мусор/CR).
    char hex[8] = {};
    size_t n = 0;
    for (const char* p = data; *p && n + 1 < sizeof(hex); ++p) {
        if (hexNibbleCam(*p) < 0) {
            break;
        }
        hex[n++] = *p;
    }
    hex[n] = '\0';
    if (n == 0) {
        return Status::Io;
    }
    uint16_t code = 0;
    st = parseHexU16(hex, code);
    if (st != Status::Ok) {
        return st;
    }
    // PDF: AGC SET = 80h. E650 live: SET OGU:80 → QGU:00 (control≠query).
    // Manual минимум 08h (0 dB) — 00h как query = AGC на студии.
    const CameraModelId id = modelId();
    const bool studio = (id == CameraModelId::E650 || id == CameraModelId::E600 ||
                         id == CameraModelId::Generic);
    if (code == kGainAgcCode || (studio && code == 0x00)) {
        agc = true;
        db = 0;
        return Status::Ok;
    }
    agc = false;
    db = static_cast<int>(code) - static_cast<int>(kGainZeroDbCode);
    return Status::Ok;
}

Status CameraDeviceBase::requestShutter(uint8_t& code)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::Shutter, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    uint16_t v = 0;
    st = parseHexU16(data, v);
    if (st != Status::Ok || v > 0xFF) {
        return Status::Io;
    }
    code = static_cast<uint8_t>(v);
    return Status::Ok;
}

Status CameraDeviceBase::requestIrisAutoManual(IrisAutoManual& mode)
{
    uint8_t v = 0;
    Status st = requestDecU8(catalog::CameraCmd::IrisAutoManual, v);
    if (st != Status::Ok) {
        return st;
    }
    if (v > static_cast<uint8_t>(IrisAutoManual::Auto)) {
        return Status::Io;
    }
    mode = static_cast<IrisAutoManual>(v);
    return Status::Ok;
}

Status CameraDeviceBase::requestManualIris(uint16_t& norm)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::ManualIrisVolume, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    uint16_t native = 0;
    st = parseHexU16(data, native);
    if (st != Status::Ok) {
        return st;
    }
    norm = camIrisToNorm(native);
    return Status::Ok;
}

Status CameraDeviceBase::requestDetail(uint8_t& mode)
{
    return requestDecU8(catalog::CameraCmd::Detail, mode);
}

Status CameraDeviceBase::requestSceneFile(uint8_t& scene)
{
    return requestDecU8(catalog::CameraCmd::SceneFile, scene);
}

Status CameraDeviceBase::requestColorBar(ColorBarMode& mode)
{
    uint8_t v = 0;
    Status st = requestDecU8(catalog::CameraCmd::ColorBar, v);
    if (st != Status::Ok) {
        return st;
    }
    if (v > static_cast<uint8_t>(ColorBarMode::ColorBar)) {
        return Status::Io;
    }
    mode = static_cast<ColorBarMode>(v);
    return Status::Ok;
}

Status CameraDeviceBase::requestMenuOnOff(Switch& mode)
{
    return requestSwitchField(catalog::CameraCmd::MenuOnOff, mode);
}

Status CameraDeviceBase::requestAutoFocus(Switch& mode)
{
    return requestSwitchField(catalog::CameraCmd::AutoFocus, mode);
}

Status CameraDeviceBase::requestFilter(uint8_t& mode)
{
    return requestDecU8(catalog::CameraCmd::FilterNd, mode);
}

Status CameraDeviceBase::requestNegPos(Switch& mode)
{
    return requestSwitchField(catalog::CameraCmd::NegPos, mode);
}

Status CameraDeviceBase::requestFieldFrame(FieldFrameMode& mode)
{
    uint8_t v = 0;
    Status st = requestDecU8(catalog::CameraCmd::FieldFrame, v);
    if (st != Status::Ok) {
        return st;
    }
    if (v > static_cast<uint8_t>(FieldFrameMode::Frame2)) {
        return Status::Io;
    }
    mode = static_cast<FieldFrameMode>(v);
    return Status::Ok;
}

Status CameraDeviceBase::requestChromaLevel(uint8_t& level)
{
    return requestDecU8(catalog::CameraCmd::ChromaLevel, level);
}

Status CameraDeviceBase::requestRGain(uint16_t& value)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::RGain, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    return parseHexU16(data, value);
}

Status CameraDeviceBase::requestBGain(uint16_t& value)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::BGain, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    return parseHexU16(data, value);
}

Status CameraDeviceBase::requestTPedestal(uint16_t& value)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::TPedestal, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    return parseHexU16(data, value);
}

Status CameraDeviceBase::requestDigitalGainUp(uint8_t& code)
{
    return requestDecU8(catalog::CameraCmd::DigitalGainUp, code);
}

Status CameraDeviceBase::requestLightPeakAvg(uint8_t& code)
{
    char raw[40] = {};
    Status st = request(catalog::CameraCmd::LightPeakAvg, raw, sizeof(raw));
    if (st != Status::Ok) {
        return st;
    }
    const char* data = dataAfterColon(raw);
    if (data == nullptr) {
        return Status::Io;
    }
    uint16_t v = 0;
    st = parseHexU16(data, v);
    if (st != Status::Ok) {
        return st;
    }
    if (v > 100) {
        return Status::Param;
    }
    code = static_cast<uint8_t>(v);
    return Status::Ok;
}

Status CameraDeviceBase::requestLightArea(uint8_t& code)
{
    return requestDecU8(catalog::CameraCmd::LightArea, code);
}

Status CameraDeviceBase::requestDnr3d(uint8_t& level)
{
    return requestDecU8(catalog::CameraCmd::Dnr3d, level);
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
    uint8_t wire = mode;
    const CameraModelId id = modelId();
    // Студия: UI/QAW → OAW control (PDF). HE/UE: код на шине совпадает.
    if (id == CameraModelId::E650 || id == CameraModelId::E600 || id == CameraModelId::Generic) {
        if (!studioAwcQueryToControl(mode, wire)) {
            return Status::Param;
        }
    }
    return sendDec1(catalog::CameraCmd::AwcMode, wire);
}

Status CameraDeviceBase::setShutter(const char* data_hex)
{
    return send(catalog::CameraCmd::Shutter, data_hex);
}

Status CameraDeviceBase::setShutterMode(uint8_t mode_0_to_f)
{
    static const char hex[] = "0123456789ABCDEF";
    char data[2] = {hex[mode_0_to_f & 0x0F], '\0'};
    return setShutter(data);
}

Status CameraDeviceBase::setSynchroScan(const char* data)
{
    return send(catalog::CameraCmd::SynchroScan, data);
}

Status CameraDeviceBase::setNegPos(bool negative)
{
    return sendDec1(
        catalog::CameraCmd::NegPos,
        static_cast<uint8_t>(negative ? Switch::On : Switch::Off));
}

Status CameraDeviceBase::setFieldFrame(FieldFrameMode mode)
{
    return sendDec1(catalog::CameraCmd::FieldFrame, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::setChromaLevel(Levels level)
{
    // OCG:00…06 (−3…+3); PDF — две цифры (не encodeDec1 одной).
    const uint8_t v = static_cast<uint8_t>(level);
    if (v > 6) {
        return Status::Param;
    }
    const char data[3] = {
        static_cast<char>('0' + (v / 10u)),
        static_cast<char>('0' + (v % 10u)),
        '\0',
    };
    return send(catalog::CameraCmd::ChromaLevel, data);
}

Status CameraDeviceBase::setRGain(const char* data)
{
    return send(catalog::CameraCmd::RGain, data);
}

Status CameraDeviceBase::setBGain(const char* data)
{
    return send(catalog::CameraCmd::BGain, data);
}

Status CameraDeviceBase::setRPedestal(const char* data)
{
    return send(catalog::CameraCmd::RPedestal, data);
}

Status CameraDeviceBase::setBPedestal(const char* data)
{
    return send(catalog::CameraCmd::BPedestal, data);
}

Status CameraDeviceBase::setTPedestal(const char* data)
{
    return send(catalog::CameraCmd::TPedestal, data);
}

Status CameraDeviceBase::setDigitalGainUp(uint8_t code)
{
    return sendDec1(catalog::CameraCmd::DigitalGainUp, code);
}

Status CameraDeviceBase::setLightPeakAvg(const char* data)
{
    return send(catalog::CameraCmd::LightPeakAvg, data);
}

Status CameraDeviceBase::setLightArea(uint8_t code)
{
    return sendDec1(catalog::CameraCmd::LightArea, code);
}

Status CameraDeviceBase::setDnr3d(uint8_t level)
{
    return sendDec1(catalog::CameraCmd::Dnr3d, level);
}

Status CameraDeviceBase::setGamma(const char* data)
{
    return setOsd(catalog::OsdItem::Gamma, data);
}

Status CameraDeviceBase::setAgcMax(const char* data)
{
    return setOsd(catalog::OsdItem::AgcMax, data);
}

Status CameraDeviceBase::setTotalDtlLevel(const char* data)
{
    return setVideoMenu(catalog::VideoMenuItem::TotalDtlLevel, data);
}

Status CameraDeviceBase::setGenlockInput(const char* data)
{
    return setVideoMenu(catalog::VideoMenuItem::GenlockInput, data);
}

Status CameraDeviceBase::setBlackShadingCorrect(bool on)
{
    return setVideoMenuSwitch(
        catalog::VideoMenuItem::BlackShadingCorrectDig, toSwitch(on));
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
    // PDF SCENE FILE: «Data of control and question is different».
    // OSF (query) 0..3 = Halogen..User; XSF (control) = OSF+1 → 1..4.
    // E650 live: XSF:0 ignored; XSF:1→OSF:0, …, XSF:4→OSF:3.
    if (scene > 8) {
        return Status::Param;
    }
    char data[4];
    Status st = encodeDecAscii(static_cast<uint8_t>(scene + 1u), data, sizeof(data));
    if (st != Status::Ok) {
        return st;
    }
    return send(catalog::CameraCmd::SceneFile, data);
}

Status CameraDeviceBase::setColorBar(ColorBarMode mode)
{
    return sendDec1(catalog::CameraCmd::ColorBar, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::setMenuOnOff(Switch mode)
{
    return sendDec1(catalog::CameraCmd::MenuOnOff, static_cast<uint8_t>(mode));
}

Status CameraDeviceBase::menuSw()
{
    return sendDec1(catalog::CameraCmd::MenuSw, 1);
}

Status CameraDeviceBase::itemSw()
{
    return sendDec1(catalog::CameraCmd::ItemSw, 1);
}

Status CameraDeviceBase::yesSw()
{
    return sendDec1(catalog::CameraCmd::YesSw, 1);
}

Status CameraDeviceBase::noSw()
{
    return sendDec1(catalog::CameraCmd::NoSw, 1);
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
