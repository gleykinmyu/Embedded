#pragma once

#include "appColors.hpp"
#include "model/mconsole.hpp"
#include "nex.hpp"
#include "nexHmiConfig.hpp"
#include "overlay/ovl.hpp"

namespace server {

struct StateColors {
    nex::Color bg;
    nex::Color text;
    nex::Color pressedBg;
    nex::Color pressedText;
};

static constexpr StateColors kStateColors[] =
{
    {AppColors::kPage,          AppColors::kBorder,   AppColors::kPage,          AppColors::kBorder},
    {AppColors::kDefault,       AppColors::kText,     AppColors::kMain,          AppColors::kTextLight},
    {AppColors::kMain,          AppColors::kTextLight,AppColors::kMain,          AppColors::kTextLight},
    {AppColors::kServerBlocked, AppColors::kTextLight,AppColors::kServerBlocked, AppColors::kTextLight},
    {AppColors::kGroupBlocked,  AppColors::kBorder,   AppColors::kGroupBlocked,  AppColors::kBorder},
};

class ConsoleBtn : public nex::comp::Button<> {
public:
    enum class State : uint8_t {
        Disabled,
        Active,
        Selected,
        Blocked,      /**< Сегментный Block (сервер). */
        GroupBlocked, /**< GRUP / шоуфайл. */
    };

    ConsoleBtn(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0,
               bool global = false) noexcept
        : Button(owner, name, id, global)
    {}

    void setState(State next) noexcept
    {
        if (!canAccess()) {
            return;
        }
        /* Local: HMI сбрасывает цвета при уходе со страницы — MCU state врёт. */
        if (global && state == next) {
            return;
        }
        const StateColors& colors = kStateColors[static_cast<size_t>(next)];
        bg.setColor(colors.bg);
        font.setColor(colors.text);
        pressed.bg.setColor(colors.pressedBg);
        pressed.font.setColor(colors.pressedText);
        state = next;
    }

    [[nodiscard]] State getState() const noexcept
    {
        return state;
    }

private:
    /** 0xFF = ещё не красили на панель (не путать с Disabled). */
    State state{static_cast<State>(0xFFu)};
};

/** ConsoleBtn с vscope=global. */
class GlobalBtn : public ConsoleBtn {
public:
    GlobalBtn(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0) noexcept
        : ConsoleBtn(owner, name, id, true)
    {}
};

/** Слот группы: преамбула `  NN - ` + имя из модели. */
class GroupBtn : public GlobalBtn {
public:
    GroupBtn(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0) noexcept
        : GlobalBtn(owner, name, id)
        , txt(*this, nex::attr::Id::Txt)
    {}

    /** @a grp == nullptr — слот за краем страницы. */
    void sync(const smcp::CGroup* grp, bool selected, bool texts) noexcept
    {
        if (grp == nullptr) {
            if (texts) {
                txt.set("");
            }
            setState(State::Disabled);
            return;
        }
        if (texts) {
            char preamble[] = "  00 - ";
            preamble[2] = static_cast<char>('0' + (grp->id() / 10u));
            preamble[3] = static_cast<char>('0' + (grp->id() % 10u));
            txt.set(preamble);
            appendText(grp->name());
        }
        if (grp->isBlocked()) {
            setState(State::GroupBlocked);
        } else if (grp->isEmpty()) {
            setState(State::Disabled);
        } else {
            setState(selected ? State::Selected : State::Active);
        }
    }

    void previewPress(bool deselect) noexcept
    {
        setState(deselect ? State::Active : State::Selected);
    }

    nex::attr::String<8> txt;
};

/** Assign слота GRUP: в Show / Block всегда Disabled. */
class AssignBtn : public GlobalBtn {
public:
    using GlobalBtn::GlobalBtn;

    /** @a grp == nullptr — слот за краем страницы. */
    void sync(const smcp::CGroup* grp, bool selected, bool enabled, bool hasSelection) noexcept
    {
        if (grp == nullptr || !enabled) {
            setState(State::Disabled);
            return;
        }
        if (grp->isEmpty()) {
            setState(hasSelection ? State::Active : State::Disabled);
        } else if (grp->isBlocked()) {
            setState(State::GroupBlocked);
        } else {
            setState(selected ? State::Selected : State::Active);
        }
    }

    void previewPress(bool deselect) noexcept
    {
        if (getState() == State::Disabled) {
            return;
        }
        setState(deselect ? State::Active : State::Selected);
    }
};

struct BrwStateColors {
    nex::Color bg;
    nex::Color txt;
    nex::Color border;
};

static constexpr BrwStateColors kBrwStateColors[] = {
    {AppColors::kPage,    AppColors::kBorder,AppColors::kBorder},
    {AppColors::kDefault, AppColors::kText,  AppColors::kBorder},
    {AppColors::kDefault,    AppColors::kText,  AppColors::kMain},
};

class BrowserBtn : public nex::comp::Textual<> {
public:
    enum class State : uint8_t {
        Disabled,
        Active,
        Selected,
    };

    BrowserBtn(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0) noexcept
        : Textual<>(owner, name, Component::Type::Text, id)
    {}

    void onTouch(const nex::msg::evTouch& e) override
    {
        if (e.state == nex::TouchState::Press && state == State::Active) {
            if (active_id != 0xFFu && active_id != id()) {
                nex::Component* comp = page.getComponent(active_id);
                if (comp != nullptr && comp->type == type) {
                    comp->onUser(0, 0);
                }
            }
            active_id = id();
            setState(State::Selected);
        }
        Textual::onTouch(e);
    }

    void onUser(uint8_t user1, uint8_t user2) override
    {
        if (user1 == 0 && user2 == 0) {
            setState(State::Active);
        }
    }

    void setState(State next) noexcept
    {
        state = next;
        const BrwStateColors& colors = kBrwStateColors[static_cast<size_t>(state)];
        bg.setColor(colors.bg);
        font.setColor(colors.txt);
        setBorderColor(colors.border);
    }

    [[nodiscard]] State getState() const noexcept
    {
        return state;
    }

    static inline uint8_t active_id = 0xFF;

private:
    void setBorderColor(nex::Color v) noexcept
    {
        nex::attr_detail::assignNumeric(*this, nex::attr::Id::Borderc, v);
    }

    State state{State::Disabled};
};

/** Дата/время в browser (bF0d…bF7d). HMI `txt_maxl` = 20. */
class FileDateText : public nex::comp::Text<nex::BG::Color, 20u> {
public:
    using nex::comp::Text<nex::BG::Color, 20u>::Text;
};

/** Счётчик страницы `n/m`. HMI `txt_maxl` = 3; MCU +1 под NUL. */
using PageLabelText = nex::comp::Text<nex::BG::Color, 4u>;

class WinchButton : public ConsoleBtn {
public:
    WinchButton(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0) noexcept
        : ConsoleBtn(owner, name, id, true)
    {}

    void setPrefix(const char* prefix) noexcept
    {
        const uint8_t idx = winchIndex();
        if (idx >= kWinchIndexCount) {
            return;
        }

        if (prefix != nullptr && prefix[0] != '\0') {
            setText(prefix);
            appendText(kWinchIndexText[idx]);
        } else {
            setText(kWinchIndexText[idx]);
        }
    }

    /**
     * Серверный Block поверх GRUP; isolate поверх Select.
     * Нет Ready → Disabled (отдельного NotReady нет).
     */
    void sync(const MConsole& con) noexcept
    {
        const uint8_t idx = winchIndex();
        if (idx >= MConsole::kMechCount) {
            setState(State::Disabled);
            return;
        }
        const smcp::IMech& mech = con.cmechs[idx];
        if (mech.isBlocked()) {
            setState(State::Blocked);
            return;
        }
        if (con.show.group.containsBlocked(idx)) {
            setState(State::GroupBlocked);
            return;
        }
        if (isolated(con, idx)) {
            setState(State::Disabled);
            return;
        }
        const bool ready = mech.status().any(smcp::IMech::Status::Ready);
        if (ready && mech.isSelectedBy(con.consoleId())) {
            setState(State::Selected);
        } else if (ready) {
            setState(State::Active);
        } else {
            setState(State::Disabled);
        }
    }

    [[nodiscard]] static bool isolated(const MConsole& con, uint8_t id) noexcept
    {
        if (!con.show.settings().isolateGroup) {
            return false;
        }
        const uint8_t active = con.shownGroup();
        if (active == MConsole::kNoActiveGroup || id >= MConsole::kMechCount) {
            return false;
        }
        return !con.show.group[active].mech().contains(id);
    }

private:
    static constexpr const char* kWinchIndexText[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
        "11", "12", "13", "14", "15", "16", "17", "18", "19", 
        "20", "21", "22", "23", "24",
    };
    static constexpr uint8_t kWinchIndexCount = sizeof(kWinchIndexText) / sizeof(kWinchIndexText[0]);

    [[nodiscard]] uint8_t winchIndex() const noexcept
    {
        const uint8_t first = nex::hmi::Page_work::b0;
        return (id() >= first) ? static_cast<uint8_t>(id() - first) : 0u;
    }
};



/** Цвета MsgBox в палитре приложения (как StatusBar + ConsoleBtn). */
inline constexpr nex::ovl::MsgBoxColors kAppMsgBoxColors{
    AppColors::kPage,      // frameBg
    AppColors::kBorder,    // frameBorder
    AppColors::kTextLight, // title
    AppColors::kText,      // body
    AppColors::kMain,      // errorTitle
    AppColors::kDefault,   // btnNormalBg
    AppColors::kBorder,    // btnNormalBorder
    AppColors::kText,      // btnNormalText
    AppColors::kMain,      // btnPressedBg
    AppColors::kDefault,      // btnHighlightBg
    AppColors::kMain,      // btnHighlightBorder
};

} // namespace server
