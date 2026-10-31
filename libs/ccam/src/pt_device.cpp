#include "devices/pt_device.hpp"

#include <cstring>

namespace ccam::devices {

PtDeviceBase::PtDeviceBase(Transport& transport)
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

Status PtDeviceBase::matchPtResponse(
    const uint8_t* data,
    size_t len,
    catalog::PtCmd cmd,
    size_t& payload_pos)
{
    const char* prefix = catalog::ptResponse(cmd);
    if (prefix == nullptr || prefix[0] == '\0') {
        return Status::Param;
    }
    if (data == nullptr) {
        return Status::Param;
    }

    const size_t n = std::strlen(prefix);
    if (len < n) {
        return Status::Io;
    }
    for (size_t i = 0; i < n; ++i) {
        if (static_cast<char>(data[i]) != prefix[i]) {
            return Status::Io;
        }
    }
    payload_pos = n;
    return Status::Ok;
}

Status PtDeviceBase::buildCommand(PtFrame& frame, const char* cmd, const char* payload)
{
    if (cmd == nullptr || cmd[0] == '\0') {
        return Status::Param;
    }

    frame.clear();

    Status st = frame.appendChar(kPtPrefix);
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

    return frame.appendChar(kPtCr);
}

Status PtDeviceBase::sendCommand(const char* cmd, const char* payload)
{
    PtFrame frame;
    Status st = buildCommand(frame, cmd, payload);
    if (st != Status::Ok) {
        return st;
    }
    return transport_.send(frame.bytes(), frame.size());
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
    return transport_.transact(
        frame.bytes(),
        frame.size(),
        response,
        response_cap,
        response_len,
        timeout_ms);
}

Status PtDeviceBase::sendDec2(catalog::PtCmd cmd, uint8_t value)
{
    if (value > 99) {
        return Status::Param;
    }

    char payload[3];
    payload[0] = static_cast<char>('0' + (value / 10));
    payload[1] = static_cast<char>('0' + (value % 10));
    payload[2] = '\0';

    return send(cmd, payload);
}

uint8_t PtDeviceBase::speedFromPercent(int8_t percent)
{
    if (percent == 0) {
        return kPtSpeedStop;
    }
    // |percent| 1..100 → величина 1..49. PDF: 01=Max .. 49=Min | 50=Stop | 51=Min .. 99=Max.
    int mag = percent < 0 ? -static_cast<int>(percent) : static_cast<int>(percent);
    if (mag > 100) {
        mag = 100;
    }
    int rate = (mag * 49 + 50) / 100; // округление к ближайшему
    if (rate < static_cast<int>(kPtSpeedMin)) {
        rate = kPtSpeedMin;
    }
    if (rate > 49) {
        rate = 49;
    }
    if (percent < 0) {
        return static_cast<uint8_t>(kPtSpeedStop - rate); // 49..01
    }
    return static_cast<uint8_t>(kPtSpeedStop + rate); // 51..99
}

int8_t PtDeviceBase::applyInvert(pt::Axis axis, int8_t percent) const
{
    if (percent == 0) {
        return 0;
    }
    bool flip = false;
    switch (axis) {
    case pt::Axis::Pan:
        flip = invert_.pan;
        break;
    case pt::Axis::Tilt:
        flip = invert_.tilt;
        break;
    case pt::Axis::Zoom:
        flip = invert_.zoom;
        break;
    case pt::Axis::Focus:
        flip = invert_.focus;
        break;
    default:
        break;
    }
    if (!flip) {
        return percent;
    }
    // percent ∈ −100..+100 — безопасное отрицание
    return static_cast<int8_t>(-percent);
}

LimitAxis PtDeviceBase::mapLimitAxis(LimitAxis axis) const
{
    switch (axis) {
    case LimitAxis::PanLeft:
        return invert_.pan ? LimitAxis::PanRight : LimitAxis::PanLeft;
    case LimitAxis::PanRight:
        return invert_.pan ? LimitAxis::PanLeft : LimitAxis::PanRight;
    case LimitAxis::TiltUp:
        return invert_.tilt ? LimitAxis::TiltDown : LimitAxis::TiltUp;
    case LimitAxis::TiltDown:
        return invert_.tilt ? LimitAxis::TiltUp : LimitAxis::TiltDown;
    }
    return axis;
}

Status PtDeviceBase::move(pt::Axis axis, int8_t speed)
{
    const uint8_t encoded = speedFromPercent(applyInvert(axis, speed));
    if (!isSpeedValid(encoded)) {
        return Status::Param;
    }
    switch (axis) {
    case pt::Axis::Pan:
        return sendDec2(catalog::PtCmd::PanSpeed, encoded);
    case pt::Axis::Tilt:
        return sendDec2(catalog::PtCmd::TiltSpeed, encoded);
    case pt::Axis::Zoom:
        return sendDec2(catalog::PtCmd::ZoomSpeed, encoded);
    case pt::Axis::Focus:
        return sendDec2(catalog::PtCmd::FocusSpeed, encoded);
    case pt::Axis::Iris:
        return sendDec2(catalog::PtCmd::IrisSpeed, encoded);
    case pt::Axis::Roll:
        return sendDec2(catalog::PtCmd::RollSpeed, encoded);
    }
    return Status::Param;
}

Status PtDeviceBase::movePT(int8_t pan, int8_t tilt)
{
    const uint8_t pan_speed = speedFromPercent(applyInvert(pt::Axis::Pan, pan));
    const uint8_t tilt_speed = speedFromPercent(applyInvert(pt::Axis::Tilt, tilt));

    char payload[5];
    payload[0] = static_cast<char>('0' + (pan_speed / 10));
    payload[1] = static_cast<char>('0' + (pan_speed % 10));
    payload[2] = static_cast<char>('0' + (tilt_speed / 10));
    payload[3] = static_cast<char>('0' + (tilt_speed % 10));
    payload[4] = '\0';

    return send(catalog::PtCmd::PanTiltSpeed, payload);
}

Status PtDeviceBase::stop()
{
    static constexpr pt::Axis kAxes[] = {
        pt::Axis::Pan,
        pt::Axis::Tilt,
        pt::Axis::Zoom,
        pt::Axis::Focus,
        pt::Axis::Roll,
        pt::Axis::Iris,
    };
    for (pt::Axis axis : kAxes) {
        Status st = move(axis, 0);
        if (st != Status::Ok) {
            return st;
        }
    }
    return Status::Ok;
}

Status PtDeviceBase::goHome()
{
    // Абсолютный home (#APC) — IntegratedPtzBase; PH — override (preset).
    return Status::Param;
}

Status PtDeviceBase::setPT(const pt::PT& /*pt*/)
{
    // #APC — IntegratedPtzBase.
    return Status::Param;
}

Status PtDeviceBase::setLens(const pt::Lens& lens)
{
    Status st = setZoom(lens.zoom);
    if (st != Status::Ok) {
        return st;
    }
    st = setFocus(lens.focus);
    if (st != Status::Ok) {
        return st;
    }
    return setIris(lens.iris);
}

Status PtDeviceBase::requestPower(uint8_t& code)
{
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;
    constexpr auto cmd = catalog::PtCmd::Power;
    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }
    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok || pos >= response_len) {
        return Status::Io;
    }
    return decodePowerResponseByte(response[pos], code);
}

Status PtDeviceBase::sendHex3(catalog::PtCmd cmd, uint16_t value)
{
    if (value > 0xFFF) {
        return Status::Param;
    }

    Frame<8> payload;
    Status st = payload.appendHex(value, 3);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendChar(0);
    if (st != Status::Ok) {
        return st;
    }
    return send(cmd, payload.data());
}

Status PtDeviceBase::queryHex3(catalog::PtCmd cmd, uint16_t& value)
{
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len, kDefaultRxTimeoutMs);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }
    return parseHexField(response, response_len, pos, 3, value);
}

Status PtDeviceBase::recallPreset(uint8_t preset)
{
    return sendDec2(catalog::PtCmd::RecallPreset, preset);
}
Status PtDeviceBase::savePreset(uint8_t preset)
{
    return sendDec2(catalog::PtCmd::SavePreset, preset);
}
Status PtDeviceBase::deletePreset(uint8_t preset)
{
    return sendDec2(catalog::PtCmd::DeletePreset, preset);
}
Status PtDeviceBase::setPresetMode(PrMode mode)
{
    return sendDec1(catalog::PtCmd::PresetMode, static_cast<uint8_t>(mode));
}
Status PtDeviceBase::setPresetSpeed(PrSpeed speed)
{
    return sendDec1(catalog::PtCmd::PresetSpeedTable, static_cast<uint8_t>(speed));
}

Status PtDeviceBase::setZoom(uint16_t pos)
{
    return sendHex3(catalog::PtCmd::ZoomPositionSet, normToPtLens(pos));
}
Status PtDeviceBase::setFocus(uint16_t pos)
{
    return sendHex3(catalog::PtCmd::FocusPositionSet, normToPtLens(pos));
}
Status PtDeviceBase::setIris(uint16_t pos)
{
    return sendHex3(catalog::PtCmd::IrisPositionSet, normToPtLens(pos));
}
Status PtDeviceBase::getZoom(uint16_t& pos)
{
    uint16_t raw = 0;
    const Status st = queryHex3(catalog::PtCmd::ZoomDaPosition, raw);
    if (st == Status::Ok) {
        pos = ptLensToNorm(raw);
    }
    return st;
}
Status PtDeviceBase::getFocus(uint16_t& pos)
{
    uint16_t raw = 0;
    const Status st = queryHex3(catalog::PtCmd::FocusDaPosition, raw);
    if (st == Status::Ok) {
        pos = ptLensToNorm(raw);
    }
    return st;
}
Status PtDeviceBase::getIris(uint16_t& pos)
{
    uint16_t raw = 0;
    // #GI → gi[Data1][Data2]; Data1 = позиция (Data2 auto пока не разбираем)
    const Status st = queryHex3(catalog::PtCmd::IrisDaPosition, raw);
    if (st == Status::Ok) {
        pos = ptLensToNorm(raw);
    }
    return st;
}
Status PtDeviceBase::setLensCtrl(Switch mode)
{
    return sendDec1(catalog::PtCmd::LensPositionCtrl, static_cast<uint8_t>(mode));
}

Status PtDeviceBase::requestPosition(pt::Position& position)
{
    constexpr auto cmd = catalog::PtCmd::GetPtzPosRaw;
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    st = parseHexField(response, response_len, pos, 4, position.pt.pan);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 4, position.pt.tilt);
    if (st != Status::Ok) {
        return st;
    }
    uint16_t z = 0;
    uint16_t f = 0;
    uint16_t i = 0;
    st = parseHexField(response, response_len, pos, 3, z);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, f);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, i);
    if (st != Status::Ok) {
        return st;
    }
    position.lens.zoom = ptLensToNorm(z);
    position.lens.focus = ptLensToNorm(f);
    position.lens.iris = ptLensToNorm(i);
    return Status::Ok;
}

Status PtDeviceBase::requestError(uint8_t& error_code)
{
    constexpr auto cmd = catalog::PtCmd::ErrorStatus;
    uint8_t response[16];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    if (pos >= response_len) {
        return Status::Io;
    }

    const int hi = hexNibble(static_cast<char>(response[pos]));
    const int lo = (pos + 1 < response_len) ? hexNibble(static_cast<char>(response[pos + 1])) : -1;

    if (hi < 0) {
        return Status::Io;
    }

    error_code = static_cast<uint8_t>((lo >= 0) ? ((hi << 4) | lo) : hi);
    return Status::Ok;
}

Status PtDeviceBase::requestPositionDisplay(pt::Position& position)
{
    constexpr auto cmd = catalog::PtCmd::GetPtzPosDisplay;
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    st = parseHexField(response, response_len, pos, 4, position.pt.pan);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 4, position.pt.tilt);
    if (st != Status::Ok) {
        return st;
    }
    uint16_t z = 0;
    uint16_t f = 0;
    uint16_t i = 0;
    st = parseHexField(response, response_len, pos, 3, z);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, f);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, i);
    if (st != Status::Ok) {
        return st;
    }
    position.lens.zoom = ptLensToNorm(z);
    position.lens.focus = ptLensToNorm(f);
    position.lens.iris = ptLensToNorm(i);
    return Status::Ok;
}

Status PtDeviceBase::requestLatestPreset(uint8_t& preset)
{
    constexpr auto cmd = catalog::PtCmd::LatestRecallPresetNo;
    uint8_t response[8];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    const int tens =
        (pos < response_len && response[pos] >= '0' && response[pos] <= '9') ? (response[pos] - '0') : 0;
    const int ones = (pos + 1 < response_len && response[pos + 1] >= '0' && response[pos + 1] <= '9')
        ? (response[pos + 1] - '0')
        : 0;
    preset = static_cast<uint8_t>(tens * 10 + ones);
    return Status::Ok;
}

Status PtDeviceBase::requestExposure(pt::Exposure& exposure)
{
    constexpr auto cmd = catalog::PtCmd::GetGainShutterNd;
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    st = parseHexField(response, response_len, pos, 3, exposure.gain);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, exposure.shutter);
    if (st != Status::Ok) {
        return st;
    }

    return parseHexField(response, response_len, pos, 3, exposure.nd);
}

Status PtDeviceBase::requestLensPositionInfo(uint16_t& zoom, uint16_t& focus, uint16_t& iris)
{
    constexpr auto cmd = catalog::PtCmd::LensPositionInfo;
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    uint16_t z = 0;
    uint16_t f = 0;
    uint16_t i = 0;
    st = parseHexField(response, response_len, pos, 3, z);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, f);
    if (st != Status::Ok) {
        return st;
    }
    st = parseHexField(response, response_len, pos, 3, i);
    if (st != Status::Ok) {
        return st;
    }
    zoom = ptLensToNorm(z);
    focus = ptLensToNorm(f);
    iris = ptLensToNorm(i);
    return Status::Ok;
}

Status PtDeviceBase::requestLandingMode(LandingMode& mode)
{
    constexpr auto cmd = catalog::PtCmd::LandingMode;
    uint8_t response[8];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }
    if (pos >= response_len) {
        return Status::Io;
    }

    const uint8_t raw = static_cast<uint8_t>(response[pos] - '0');
    if (raw > static_cast<uint8_t>(LandingMode::Soft)) {
        return Status::Io;
    }
    mode = static_cast<LandingMode>(raw);
    return Status::Ok;
}

Status PtDeviceBase::requestSoftwareVersion(char* out, size_t out_cap)
{
    if (out == nullptr || out_cap == 0) {
        return Status::Param;
    }

    constexpr auto cmd = catalog::PtCmd::SoftwareVersionQuery;
    uint8_t response[kPtFrameMax];
    size_t response_len = 0;

    Status st = transact(cmd, nullptr, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }

    size_t copy_len = response_len;
    if (copy_len > 0 && response[copy_len - 1] == kPtCr) {
        --copy_len;
    }
    if (copy_len <= pos) {
        out[0] = '\0';
        return Status::Ok;
    }

    copy_len -= pos;
    const size_t n = (copy_len < out_cap - 1) ? copy_len : out_cap - 1;
    std::memcpy(out, &response[pos], n);
    out[n] = '\0';
    return Status::Ok;
}

Status PtDeviceBase::toggleLimitation(LimitAxis axis, Limitation* new_state)
{
    // PDF / HE IP: #L[1..4] — toggle лимита оси в ТЕКУЩЕЙ позиции.
    // Ответ l0=Release / l1=Set (цифра — состояние, не ось).
    // axis — логическая сторона (с учётом AxisInvert → физическая #L).
    const uint8_t ax = static_cast<uint8_t>(mapLimitAxis(axis));
    if (ax < 1 || ax > 4) {
        return Status::Param;
    }

    char payload[2];
    payload[0] = static_cast<char>('0' + ax);
    payload[1] = '\0';

    constexpr auto cmd = catalog::PtCmd::LimitationSet;
    uint8_t rx[16];
    size_t n = 0;
    Status st = transact(cmd, payload, rx, sizeof(rx), &n);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(rx, n, cmd, pos);
    if (st != Status::Ok || pos >= n || (rx[pos] != '0' && rx[pos] != '1')) {
        return Status::Io;
    }
    if (new_state != nullptr) {
        *new_state = (rx[pos] == '1') ? Limitation::Set : Limitation::Release;
    }
    return Status::Ok;
}

Status PtDeviceBase::setTiltRange(TiltRange range)
{
    return sendDec1(catalog::PtCmd::TiltRange, static_cast<uint8_t>(range));
}
Status PtDeviceBase::setTallyEnable(Switch mode)
{
    return sendDec1(catalog::PtCmd::TallyEnable, static_cast<uint8_t>(mode));
}

Status PtDeviceBase::setWirelessControl(Switch mode)
{
    return sendDec1(catalog::PtCmd::WirelessControl, static_cast<uint8_t>(mode));
}
Status PtDeviceBase::setFan(FanMode mode)
{
    return sendDec1(catalog::PtCmd::Fan, static_cast<uint8_t>(mode));
}
Status PtDeviceBase::setFan2(FanMode mode)
{
    return sendDec1(catalog::PtCmd::Fan2, static_cast<uint8_t>(mode));
}
Status PtDeviceBase::setWiper(WiperMode mode)
{
    return sendDec1(catalog::PtCmd::WiperCtrl, static_cast<uint8_t>(mode));
}
Status PtDeviceBase::setWasher(Switch mode)
{
    return sendDec1(catalog::PtCmd::Washer, static_cast<uint8_t>(mode));
}
Status PtDeviceBase::setOption(Switch mode)
{
    return sendDec1(catalog::PtCmd::Option, static_cast<uint8_t>(mode));
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
    char payload[3];
    Status st = encodeDec1(value, payload, sizeof(payload));
    if (st != Status::Ok) {
        return st;
    }
    return send(cmd, payload);
}

Status PtDeviceBase::send(catalog::PtCmd cmd, const char* payload)
{
    return sendCommand(catalog::ptControl(cmd), payload);
}

Status PtDeviceBase::transact(
    catalog::PtCmd cmd,
    const char* payload,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len,
    uint32_t timeout_ms)
{
    return transactCommand(
        catalog::ptControl(cmd), payload, response, response_cap, response_len, timeout_ms);
}

} // namespace ccam::devices
