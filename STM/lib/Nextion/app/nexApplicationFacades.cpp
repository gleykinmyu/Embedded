#include "nexApplicationFacades.hpp"
#include "nexApplication.hpp"
#include "nexSysVars.hpp"

namespace nex {

namespace {

constexpr Literal kSysSleep{"sleep"};
constexpr Literal kSysUssp{"ussp"};
constexpr Literal kSysThsp{"thsp"};
constexpr Literal kSysThup{"thup"};
constexpr Literal kSysWup{"wup"};
constexpr Literal kSysUsup{"usup"};
constexpr Literal kSysEql{"eql"};
constexpr Literal kSysEqm{"eqm"};
constexpr Literal kSysEqh{"eqh"};
constexpr Literal kSysEq0{"eq0"};
constexpr Literal kSysEq1{"eq1"};
constexpr Literal kSysEq2{"eq2"};
constexpr Literal kSysEq3{"eq3"};
constexpr Literal kSysEq4{"eq4"};
constexpr Literal kSysEq5{"eq5"};
constexpr Literal kSysEq6{"eq6"};
constexpr Literal kSysEq7{"eq7"};
constexpr Literal kSysEq8{"eq8"};
constexpr Literal kSysEq9{"eq9"};
constexpr Literal kSysVolume{"volume"};
constexpr Literal kSysAudio0{"audio0"};
constexpr Literal kSysAudio1{"audio1"};
constexpr Literal kSysSendxy{"sendxy"};

const Literal& audioChannelSysName(uint8_t channel) noexcept {
    return (channel == 0u) ? kSysAudio0 : kSysAudio1;
}

void setAudioChannelState(Application& app, uint8_t channel, int32_t state) noexcept {
    if (channel > 1u)
        return;
    enqueueSysVarNumericAssign(app, audioChannelSysName(channel), state);
}

} // namespace

// --- AppEeprom --------------------------------------------------------------

AppEeprom::AppEeprom(Application& a) noexcept : _app(a) {}

void AppEeprom::write(const AttrRef& variableOrConstant, uint32_t eepromStart) const noexcept {
    _app.enqueue(Transaction{cmd::Eeprom::write(variableOrConstant, eepromStart), 0u, 0u});
}

void AppEeprom::read(const AttrRef& destVariable, uint32_t eepromStart) const noexcept {
    _app.enqueue(Transaction{cmd::Eeprom::read(destVariable, eepromStart), 0u, 0u});
}

void AppEeprom::write_t(uint32_t eepromStart, const uint8_t* buffer, uint32_t byteCount) const noexcept {
    (void)buffer;
    _app.enqueue(
        Transaction{cmd::Eeprom::writeT(eepromStart, byteCount), 0u, 0u, 0u, Transaction::Kind::TransparentTx});
}

void AppEeprom::read_t(uint32_t eepromStart, uint8_t* buffer, uint32_t byteCount) const noexcept {
    (void)buffer;
    _app.enqueue(Transaction{cmd::Eeprom::readT(eepromStart, byteCount), 0u, 0u, 0u, Transaction::Kind::RawDataRx});
}

// --- AppFileSystem ----------------------------------------------------------

AppFileSystem::AppFileSystem(Application& a) noexcept : _app(a) {}

void AppFileSystem::file_remove(const char* pathQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::File::remove(pathQuoted), 0u, 0u});
}

void AppFileSystem::file_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::File::rename(pathFromQuoted, pathToQuoted), 0u, 0u});
}

void AppFileSystem::file_find(const char* pathQuoted, const AttrRef& dstNumAttr) const noexcept {
    _app.enqueue(Transaction{cmd::File::find(pathQuoted, dstNumAttr), 0u, 0u});
}

void AppFileSystem::file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept {
    _app.enqueue(Transaction{cmd::File::create(pathQuoted, reservedSize), 0u, 0u});
}

void AppFileSystem::file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount,
    uint32_t crcOption) const noexcept {
    (void)buffer;
    _app.enqueue(
        Transaction{cmd::File::read(pathQuoted, offset, byteCount, crcOption), 0u, 0u, 0u, Transaction::Kind::RawDataRx});
}

void AppFileSystem::file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept {
    (void)buffer;
    _app.enqueue(Transaction{cmd::File::writeT(pathQuoted, byteCount), 0u, 0u, 0u, Transaction::Kind::TransparentTx});
}

void AppFileSystem::dir_remove(const char* pathQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::remove(pathQuoted), 0u, 0u});
}

void AppFileSystem::dir_create(const char* pathQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::create(pathQuoted), 0u, 0u});
}

void AppFileSystem::dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::rename(pathFromQuoted, pathToQuoted), 0u, 0u});
}

void AppFileSystem::dir_find(const char* pathQuoted, const AttrRef& dstNumAttr) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::find(pathQuoted, dstNumAttr), 0u, 0u});
}

// --- AppAudio ---------------------------------------------------------------

AppAudio::AppAudio(Application& a) noexcept
    : _app(a)
    , ch{AppAudioChannel(a, 0u), AppAudioChannel(a, 1u)}
{}

void AppAudio::setEqLow(uint8_t level) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysEql, static_cast<int32_t>(level));
}

void AppAudio::setEqMid(uint8_t level) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysEqm, static_cast<int32_t>(level));
}

void AppAudio::setEqHigh(uint8_t level) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysEqh, static_cast<int32_t>(level));
}

void AppAudio::setEqBand(uint8_t band, uint8_t level) const noexcept {
    const Literal* name = nullptr;
    switch (band) {
    case 0: name = &kSysEq0; break;
    case 1: name = &kSysEq1; break;
    case 2: name = &kSysEq2; break;
    case 3: name = &kSysEq3; break;
    case 4: name = &kSysEq4; break;
    case 5: name = &kSysEq5; break;
    case 6: name = &kSysEq6; break;
    case 7: name = &kSysEq7; break;
    case 8: name = &kSysEq8; break;
    case 9: name = &kSysEq9; break;
    default: return;
    }
    enqueueSysVarNumericAssign(_app, *name, static_cast<int32_t>(level));
}

void AppAudio::setVolume(uint8_t level) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysVolume, static_cast<int32_t>(level));
}

// --- AppAudioChannel --------------------------------------------------------

AppAudioChannel::AppAudioChannel(Application& a, uint8_t id) noexcept : _app(a), _id(id) {}

void AppAudioChannel::play(uint32_t resourceId, uint32_t loop01) const noexcept {
    if (_id > 1u)
        return;
    _app.enqueue(Transaction{cmd::Play(_id, resourceId, loop01), 0u, 0u});
}

void AppAudioChannel::stop() const noexcept {
    setAudioChannelState(_app, _id, 0);
}

void AppAudioChannel::pause() const noexcept {
    setAudioChannelState(_app, _id, 2);
}

void AppAudioChannel::resume() const noexcept {
    setAudioChannelState(_app, _id, 1);
}

// --- AppSleep ---------------------------------------------------------------

AppSleep::AppSleep(Application& a) noexcept : _app(a) {}

void AppSleep::enter() const noexcept {
    enqueueSysVarNumericAssign(_app, kSysSleep, 1);
}

void AppSleep::exit() const noexcept {
    enqueueSysVarNumericAssign(_app, kSysSleep, 0);
}

void AppSleep::noSerialTimer(uint16_t seconds) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysUssp, static_cast<int32_t>(seconds));
}

void AppSleep::noTouchTimer(uint16_t seconds) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysThsp, static_cast<int32_t>(seconds));
}

void AppSleep::wakeOnTouch(bool wake) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysThup, wake ? 1 : 0);
}

void AppSleep::wakeUpPage(uint8_t pageId) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysWup, static_cast<int32_t>(pageId));
}

void AppSleep::wakeOnSerial(bool wake) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysUsup, wake ? 1 : 0);
}

// --- AppTouch ---------------------------------------------------------------

AppTouch::AppTouch(Application& a) noexcept
    : _app(a)
    , tch{SysVar<int16_t>(a, "tch0", SysVarTag::Tch0), SysVar<int16_t>(a, "tch1", SysVarTag::Tch1),
        SysVar<int16_t>(a, "tch2", SysVarTag::Tch2), SysVar<int16_t>(a, "tch3", SysVarTag::Tch3)}
{}

void AppTouch::sendXY(bool enable) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysSendxy, enable ? 1 : 0);
}

void AppTouch::touchSwitch(bool enabled) const noexcept {
    _app.enqueue(Transaction{cmd::Component::tsw(cmd::TopLayer, enabled), 0u, 0u});
}

void AppTouch::calibrate() const noexcept {
    _app.enqueue(Transaction{cmd::System::touchJ(), 0u, 0u});
}

} // namespace nex
