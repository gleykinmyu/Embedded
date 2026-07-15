#pragma once

#include "nex.hpp"
#include "nexHmiConfig.hpp"
#include "overlay/ovl.hpp"

namespace server {

struct AppColors {
    static constexpr nex::Color kPage{4258u};
    static constexpr nex::Color kDefault{10565u};
    static constexpr nex::Color kMain{64800u};
    static constexpr nex::Color kBlocked{43370u};
    static constexpr nex::Color kBorder{21130u};

    static constexpr nex::Color kText{61277u};
    static constexpr nex::Color kTextLight{65535u};
};

struct StateColors {
    nex::Color bg;
    nex::Color text;
    nex::Color pressedBg;
    nex::Color pressedText;
};

static constexpr StateColors kStateColors[] =
{
    {AppColors::kPage,    AppColors::kBorder,   AppColors::kPage,    AppColors::kBorder},
    {AppColors::kDefault, AppColors::kText,     AppColors::kMain,    AppColors::kTextLight},
    {AppColors::kMain,    AppColors::kTextLight,AppColors::kMain,    AppColors::kTextLight},
    {AppColors::kBlocked, AppColors::kBorder,   AppColors::kBlocked, AppColors::kBorder},
};

class ConsoleBtn : public nex::comp::Button<> {
public:
    enum class State : uint8_t {
        Disabled,
        Active,
        Selected,
        Blocked,
    };

    ConsoleBtn(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0) noexcept
        : Button(owner, name, id)
    {}

    void setState(State next) noexcept
    {
        state = next;
        const StateColors& colors = kStateColors[static_cast<size_t>(state)];
        bg.setColor(colors.bg);
        font.setColor(colors.text);
        pressed.bg.setColor(colors.pressedBg);
        pressed.font.setColor(colors.pressedText);
    }

    [[nodiscard]] State getState() const noexcept
    {
        return state;
    }

private:
    State state{State::Disabled};
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

/** Строка даты/времени в browser (bF0d..bF7d), txt до 20 символов. */
class FileDateText : public nex::comp::Text<nex::BG::Color, 20u> {
public:
    using nex::comp::Text<nex::BG::Color, 20u>::Text;
};

class GroupBtn : public ConsoleBtn {
public:
    nex::attr::String<8> txt;

    GroupBtn(nex::IPage& owner, const nex::Literal& name, uint8_t id = 0) noexcept
        : ConsoleBtn(owner, name, id)
        , txt{*this, nex::attr::Id::Txt}
    {}
};

class WinchButton : public ConsoleBtn {
public:
    using ConsoleBtn::ConsoleBtn;

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
