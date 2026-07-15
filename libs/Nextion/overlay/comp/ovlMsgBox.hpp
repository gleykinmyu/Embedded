#pragma once

#include <cstdarg>
#include <cstdint>

#include "../../core/nexMessages.hpp"
#include "../../core/nexTypes.hpp"
#include "../ovlStyle.hpp"
#include "../ovlWidget.hpp"
#include "ovlButton.hpp"

namespace nex {

namespace ovl {

struct MsgBoxFrameStyle : TextBoxStyle {
    uint16_t pad;
    uint16_t titleH;
};

struct MsgBoxButtonLayout {
    uint16_t h;
    uint16_t gap;
    uint16_t rowGap;
    uint16_t bottomLift;
    uint16_t w;
    uint16_t wMulti;
    uint16_t wCancel;
    Font font;
};

struct MsgBoxButtonLabels {
    const char* ok;
    const char* yes;
    const char* no;
    const char* cancel;
};

struct MsgBoxTitleLabels {
    const char* error;
    const char* message;
};

struct MsgBoxButtonStyles {
    ButtonStyle normal;
    ButtonStyle highlighted;
};

/** Цветовая схема MsgBox. */
struct MsgBoxColors {
    Color frameBg;
    Color frameBorder;
    Color title;
    Color body;
    Color errorTitle;
    Color btnNormalBg;
    Color btnNormalBorder;
    Color btnNormalText;
    Color btnPressedBg;
    Color btnHighlightBg;
    Color btnHighlightBorder;
};

inline constexpr MsgBoxButtonLayout kMsgBoxBtn{
    56u,
    10u,
    8u,
    8u,
    140u,
    96u,
    140u,
    Font{0u, 32u},
};

inline constexpr MsgBoxFrameStyle kMsgBoxFrame{
    TextBoxStyle{
        Color(0x4208u),
        Color(0x7BEFu),
        4u,
        0u,
        32u,
        Color::std::White,
    },
    10u,
    44u,
};

inline constexpr MsgBoxButtonLabels kMsgBoxLabels{
    "OK",
    "Yes",
    "No",
    "Cancel",
};

inline constexpr MsgBoxTitleLabels kMsgBoxTitles{
    "Error",
    "Message",
};

inline constexpr MsgBoxButtonStyles kMsgBoxBtnStyles{
    ButtonStyle{
        TextBoxStyle{Color(0x528Au), Color(0x7BEFu), 3u, kMsgBoxBtn.font, Color::std::White},
        TextBoxStyle{Color::std::Green, Color(0x7BEFu), 0u, kMsgBoxBtn.font, Color::std::White},
    },
    ButtonStyle{
        TextBoxStyle{Color(0x7BEFu), Color::std::White, 2u, kMsgBoxBtn.font, Color::std::White},
        TextBoxStyle{Color::std::Green, Color(0x7BEFu), 0u, kMsgBoxBtn.font, Color::std::White},
    },
};

inline constexpr MsgBoxColors kMsgBoxColors{
    kMsgBoxFrame.bg,
    kMsgBoxFrame.border.color,
    kMsgBoxFrame.font.color,
    kMsgBoxFrame.font.color,
    Color::std::Yellow,
    kMsgBoxBtnStyles.normal.normal.bg,
    kMsgBoxBtnStyles.normal.normal.border.color,
    kMsgBoxBtnStyles.normal.normal.font.color,
    kMsgBoxBtnStyles.normal.pressed.bg,
    kMsgBoxBtnStyles.highlighted.normal.bg,
    kMsgBoxBtnStyles.highlighted.normal.border.color,
};

} // namespace ovl
} // namespace nex

#include "ovlMsgButton.hpp"

namespace nex {

class OvlApp;

namespace ovl {

/** Модальный диалог McUI: `Widget` + `Button`, `show` через `Overlay`. */
class MsgBox : public Widget {
public:
    using FrameStyle = MsgBoxFrameStyle;
    using ButtonLayout = MsgBoxButtonLayout;
    using ButtonLabels = MsgBoxButtonLabels;
    using TitleLabels = MsgBoxTitleLabels;
    using ButtonStyles = MsgBoxButtonStyles;
    using Colors = MsgBoxColors;

    static constexpr uint16_t kTextCap = 80u;
    static constexpr uint16_t kTitleCap = 32u;
    static constexpr uint8_t kBtnCount = 4u;

    enum class Preset : uint8_t {
        OK = 0,
        OKCancel,
        YesNo,
        YesNoCancel,
    };
    using Action = msg::evMsgBox::Action;

    [[nodiscard]] static Action defaultForPreset(Preset preset) noexcept;
    [[nodiscard]] static const char* actionCstr(Action action) noexcept;
    [[nodiscard]] static const char* presetCstr(Preset preset) noexcept;

    explicit MsgBox(OvlApp& app, const MsgBoxColors& colors = kMsgBoxColors) noexcept;

    /** Title `"Message"`, `Preset::OK`, `tag` 0. */
    void show(const char* fmt, ...) noexcept;

    /** Title `"Message"`, `tag` 0; default-кнопка из `preset`. */
    void show(Preset preset, const char* fmt, ...) noexcept;

    /** `defaultAction == Action::None` → `defaultForPreset(preset)`. */
    void show(const char* title, Preset preset, uint8_t tag, Action defaultAction, const char* fmt, ...) noexcept;

    void showError(Preset preset = Preset::OK, uint8_t tag = msg::evMsgBox::kTagError) noexcept;
    void showError(Preset preset, Action defaultAction, uint8_t tag = msg::evMsgBox::kTagError) noexcept;

    [[nodiscard]] const MsgBoxColors& colors() const noexcept { return _colors; }

    void setRoute(Route route) noexcept;

private:
    void layout() noexcept override;
    void drawBackground(const AppCanvas& cs) const override;
    void drawBackgroundRegion(const AppCanvas& cs, Region clip) const override;
    void onClick(Object* target) noexcept override;

    void applyShow(Preset preset, Action defaultAction, uint8_t tag) noexcept;
    void setTitle(const char* title) noexcept;
    void formatBody(const char* fmt, va_list ap) noexcept;
    void showV(const char* title, Preset preset, Action defaultAction, uint8_t tag, const char* fmt,
        va_list ap) noexcept;
    void present() noexcept;

    [[nodiscard]] MsgBoxFrameStyle resolvedFrame() const noexcept;
    [[nodiscard]] MsgBoxButtonStyles resolvedButtonStyles() const noexcept;
    [[nodiscard]] Color resolvedTitleColor() const noexcept;

    void fit(const Rect& screen) noexcept;
    [[nodiscard]] bool hasScreen() const noexcept { return _screenSize.w != 0u && _screenSize.h != 0u; }

    OvlApp& _app;
    Rect _screenSize{};
    MsgButton _btns[kBtnCount];

    msg::evMsgBox _ev{};
    Preset _preset = Preset::OK;
    Action _defaultAction = Action::Ok;
    bool _routePinned = false;

    MsgBoxColors _colors;

    char _title[kTitleCap]{};
    char _text[kTextCap]{};
};

} // namespace ovl
} // namespace nex
