#include "settingsPage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"
#include "board.hpp"
#include "phl/rtc.hpp"

namespace server {

namespace {

struct ClampResult {
    int32_t value;
    bool clamped;
};

[[nodiscard]] ClampResult clampCheck(int32_t v, int32_t lo, int32_t hi) noexcept
{
    if (v < lo) {
        return {lo, true};
    }
    if (v > hi) {
        return {hi, true};
    }
    return {v, false};
}

[[nodiscard]] bool applyClamped(Number<>& field, int32_t lo, int32_t hi, int32_t& out) noexcept
{
    const ClampResult r = clampCheck(field.val, lo, hi);
    out = r.value;
    if (r.clamped) {
        field.val = r.value;
    }
    return r.clamped;
}

void showClampMsg(Application& ui, uint8_t tag) noexcept
{
    ui.showUtf8Msg(uiMsg::kTitleSettings, nex::ovl::MsgBox::Preset::OK, tag,
        nex::ovl::MsgBox::Action::Ok, uiMsg::kDateTimeClamped);
}

} // namespace

SettingsPage::SettingsPage(nex::IAppUI& app) noexcept
    : Page<23>(app, HMI_COMP_OBJNAME(settings), PG::kPageId)
{}

Application& SettingsPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void SettingsPage::noteReturnFromKeybd() noexcept
{
    _returnFromKeybd = true;
}

void SettingsPage::onLoad()
{
    if (_returnFromKeybd) {
        _returnFromKeybd = false;
        beginValidateAfterKeybd();
        return;
    }

    _pending = Pending::None;
    _pendingGets = 0u;
    loadRtcToUi();
    swGr0.val = console.settings().isolateGroup;
}

void SettingsPage::loadRtcToUi() noexcept
{
    PHL::DateTime dt{};
    if (!board.rtc.isReady() || !board.rtc.get(dt)) {
        return;
    }

    nThour.val = static_cast<int32_t>(dt.hour);
    nTmin.val = static_cast<int32_t>(dt.minute);
    nTsec.val = static_cast<int32_t>(dt.second);
    nDday.val = static_cast<int32_t>(dt.day);
    nDmon.val = static_cast<int32_t>(dt.month);
    nDyear.val = static_cast<int32_t>(dt.year);
}

void SettingsPage::requestAllFields() noexcept
{
    _pendingGets = 6u;
    nThour.val.get();
    nTmin.val.get();
    nTsec.val.get();
    nDday.val.get();
    nDmon.val.get();
    nDyear.val.get();
}

bool SettingsPage::clampAllFields() noexcept
{
    int32_t hour = 0;
    int32_t minute = 0;
    int32_t second = 0;
    int32_t day = 0;
    int32_t month = 0;
    int32_t year = 0;

    bool clamped = false;
    clamped |= applyClamped(nThour, 0, 23, hour);
    clamped |= applyClamped(nTmin, 0, 59, minute);
    clamped |= applyClamped(nTsec, 0, 59, second);
    clamped |= applyClamped(nDday, 1, 31, day);
    clamped |= applyClamped(nDmon, 1, 12, month);
    clamped |= applyClamped(nDyear, 2000, 2099, year);
    return clamped;
}

void SettingsPage::beginValidateAfterKeybd() noexcept
{
    _pending = Pending::ValidateAfterKeybd;
    requestAllFields();
}

void SettingsPage::commitValidateAfterKeybd() noexcept
{
    _pending = Pending::None;
    _pendingGets = 0u;

    if (clampAllFields()) {
        showClampMsg(ui(), Msg::AfterClampField);
    }
}

void SettingsPage::beginApply() noexcept
{
    _pending = Pending::ApplyAll;
    _pendingGets = 7u;
    nThour.val.get();
    nTmin.val.get();
    nTsec.val.get();
    nDday.val.get();
    nDmon.val.get();
    nDyear.val.get();
    swGr0.val.get();
}

void SettingsPage::commitApply() noexcept
{
    _pending = Pending::None;
    _pendingGets = 0u;

    int32_t hour = 0;
    int32_t minute = 0;
    int32_t second = 0;
    int32_t day = 0;
    int32_t month = 0;
    int32_t year = 0;

    bool clamped = false;
    clamped |= applyClamped(nThour, 0, 23, hour);
    clamped |= applyClamped(nTmin, 0, 59, minute);
    clamped |= applyClamped(nTsec, 0, 59, second);
    clamped |= applyClamped(nDday, 1, 31, day);
    clamped |= applyClamped(nDmon, 1, 12, month);
    clamped |= applyClamped(nDyear, 2000, 2099, year);

    PHL::DateTime dt{};
    dt.hour = static_cast<uint8_t>(hour);
    dt.minute = static_cast<uint8_t>(minute);
    dt.second = static_cast<uint8_t>(second);
    dt.day = static_cast<uint8_t>(day);
    dt.month = static_cast<uint8_t>(month);
    dt.year = static_cast<uint16_t>(year);

    if (board.rtc.isReady()) {
        (void)board.rtc.set(dt);
    }

    {
        MConsole::Settings s = console.settings();
        s.isolateGroup = static_cast<bool>(swGr0.val);
        console.setSettings(s);
    }

    if (clamped) {
        showClampMsg(ui(), Msg::AfterClampApply);
        return;
    }

    finishToWork();
}

void SettingsPage::finishToWork() noexcept
{
    _returnFromKeybd = false;
    _pending = Pending::None;
    _pendingGets = 0u;
    ui().switchPage(ui().work);
}

void SettingsPage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PS = nex::hmi::Page_settings;
    switch (e.route.comp) {
    case PS::bCancel:
    case PS::mBack:
        finishToWork();
        break;

    case PS::bAction:
        beginApply();
        break;

    default:
        break;
    }
}

void SettingsPage::onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag)
{
    Page::onResponse(response, route, tag);

    if (_pending == Pending::None || _pendingGets == 0u) {
        return;
    }

    using PS = nex::hmi::Page_settings;
    switch (route.comp) {
    case PS::nThour:
    case PS::nTmin:
    case PS::nTsec:
    case PS::nDday:
    case PS::nDmon:
    case PS::nDyear:
    case PS::swGr0:
        if (--_pendingGets != 0u) {
            return;
        }
        if (_pending == Pending::ValidateAfterKeybd) {
            commitValidateAfterKeybd();
        } else if (_pending == Pending::ApplyAll) {
            commitApply();
        }
        break;
    default:
        break;
    }
}

void SettingsPage::onMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.tag == Msg::AfterClampApply) {
        finishToWork();
    }
}

} // namespace server
