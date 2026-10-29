#include "devices/integrated_ptz.hpp"

#include "transport/frame.hpp"

namespace ccam::devices {

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

} // namespace

Status IntegratedPtzBase::setPT(const pt::PT& pt)
{
    Frame<16> payload;
    Status st = payload.appendHex(pt.pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(pt.tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendChar(0);
    if (st != Status::Ok) {
        return st;
    }

    uint8_t rx[24];
    size_t n = 0;
    st = transact(
        catalog::PtCmd::PanTiltAbsolute, payload.data(), rx, sizeof(rx), &n, 300);
    if (st == Status::Timeout) {
        return Status::Ok; // тишина = норма для P/T
    }
    if (st != Status::Ok) {
        return st;
    }
    if (n >= 3 && rx[0] == 'e' && rx[1] == 'R') {
        return Status::Nack;
    }
    return Status::Ok;
}

Status IntegratedPtzBase::goHome()
{
    return setPT(pt::PT{kPtPosCenter, kPtPosCenter});
}

Status IntegratedPtzBase::setPTwSpeed(
    uint16_t pan,
    uint16_t tilt,
    uint8_t preset_speed,
    PrSpeed speed_table)
{
    Frame<16> payload;
    Status st = payload.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(preset_speed, 2);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(static_cast<uint8_t>(speed_table), 1);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendChar(0);
    if (st != Status::Ok) {
        return st;
    }
    return send(catalog::PtCmd::PanTiltAbsoluteSpeed, payload.data());
}

Status IntegratedPtzBase::relativePosition(uint16_t pan, uint16_t tilt)
{
    Frame<16> payload;
    Status st = payload.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendChar(0);
    if (st != Status::Ok) {
        return st;
    }
    return send(catalog::PtCmd::PanTiltRelative, payload.data());
}

Status IntegratedPtzBase::relPTwSpeed(
    uint16_t pan,
    uint16_t tilt,
    uint8_t preset_speed,
    PrSpeed speed_table)
{
    Frame<16> payload;
    Status st = payload.appendHex(pan, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(tilt, 4);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(preset_speed, 2);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendHex(static_cast<uint8_t>(speed_table), 1);
    if (st != Status::Ok) {
        return st;
    }
    st = payload.appendChar(0);
    if (st != Status::Ok) {
        return st;
    }
    return send(catalog::PtCmd::PanTiltRelativeSpeed, payload.data());
}

Status IntegratedPtzBase::requestPresetEntryBank(uint8_t bank_0_to_2, uint64_t& bits40)
{
    if (bank_0_to_2 > 2) {
        return Status::Param;
    }

    char payload[3];
    payload[0] = '0';
    payload[1] = static_cast<char>('0' + bank_0_to_2);
    payload[2] = '\0';

    constexpr auto cmd = catalog::PtCmd::PresetEntryConfirm;
    uint8_t response[24];
    size_t response_len = 0;
    Status st = transact(cmd, payload, response, sizeof(response), &response_len);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(response, response_len, cmd, pos);
    if (st != Status::Ok) {
        return st;
    }
    if (pos + 12 > response_len) {
        return Status::Io;
    }
    uint16_t bank_echo = 0;
    st = parseHexField(response, response_len, pos, 2, bank_echo);
    if (st != Status::Ok || bank_echo != bank_0_to_2) {
        return Status::Io;
    }

    uint64_t bits = 0;
    for (uint16_t i = 0; i < 10; ++i) {
        if (pos >= response_len) {
            return Status::Io;
        }
        const int nib = hexNibble(static_cast<char>(response[pos++]));
        if (nib < 0) {
            return Status::Io;
        }
        bits = (bits << 4) | static_cast<uint64_t>(nib);
    }
    bits40 = bits;
    return Status::Ok;
}

Status IntegratedPtzBase::requestPresetMap(bool occupied[100])
{
    if (occupied == nullptr) {
        return Status::Param;
    }
    for (int i = 0; i < 100; ++i) {
        occupied[i] = false;
    }

    Status first_err = Status::Ok;
    bool any_ok = false;
    for (uint8_t bank = 0; bank < 3; ++bank) {
        uint64_t bits = 0;
        const Status st = requestPresetEntryBank(bank, bits);
        if (st != Status::Ok) {
            if (first_err == Status::Ok) {
                first_err = st;
            }
            continue;
        }
        any_ok = true;
        const int count = (bank == 2) ? 20 : 40;
        const int base = static_cast<int>(bank) * 40;
        for (int i = 0; i < count; ++i) {
            occupied[base + i] = ((bits >> i) & 1ull) != 0;
        }
    }
    return any_ok ? Status::Ok : first_err;
}

Status IntegratedPtzBase::setLimitationControl(LimitAxis axis, Limitation action)
{
    const uint8_t ax = static_cast<uint8_t>(mapLimitAxis(axis));
    if (ax < 1 || ax > 4) {
        return Status::Param;
    }
    const uint8_t act = static_cast<uint8_t>(action);
    if (act > 1) {
        return Status::Param;
    }

    char payload[3];
    payload[0] = static_cast<char>('0' + ax);
    payload[1] = static_cast<char>('0' + act);
    payload[2] = '\0';

    constexpr auto cmd = catalog::PtCmd::LimitationControl;
    uint8_t rx[16];
    size_t n = 0;
    Status st = transact(cmd, payload, rx, sizeof(rx), &n);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(rx, n, cmd, pos);
    if (st != Status::Ok || pos + 1 >= n ||
        static_cast<uint8_t>(rx[pos] - '0') != ax ||
        (rx[pos + 1] != '0' && rx[pos + 1] != '1')) {
        return Status::Io;
    }
    return Status::Ok;
}

Status IntegratedPtzBase::requestLimitation(LimitAxis axis, Limitation& state)
{
    const uint8_t ax = static_cast<uint8_t>(mapLimitAxis(axis));
    if (ax < 1 || ax > 4) {
        return Status::Param;
    }

    char payload[2];
    payload[0] = static_cast<char>('0' + ax);
    payload[1] = '\0';

    constexpr auto cmd = catalog::PtCmd::LimitationControl;
    uint8_t rx[16];
    size_t n = 0;
    Status st = transact(cmd, payload, rx, sizeof(rx), &n);
    if (st != Status::Ok) {
        return st;
    }

    size_t pos = 0;
    st = matchPtResponse(rx, n, cmd, pos);
    if (st != Status::Ok || pos + 1 >= n ||
        static_cast<uint8_t>(rx[pos] - '0') != ax) {
        return Status::Io;
    }
    if (rx[pos + 1] == '0') {
        state = Limitation::Release;
        return Status::Ok;
    }
    if (rx[pos + 1] == '1') {
        state = Limitation::Set;
        return Status::Ok;
    }
    return Status::Io;
}

} // namespace ccam::devices
