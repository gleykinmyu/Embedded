#pragma once

#include "nex.hpp"
#include "nexHmiConfig.hpp"

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

} // namespace server
