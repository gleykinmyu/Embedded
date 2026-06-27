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
constexpr Literal kSysSendxy{"sendxy"};

} // namespace

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
