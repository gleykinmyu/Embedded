#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include "../core/nexTypes.hpp"
#include "../core/nexMessages.hpp"
#include "nexCanvas.hpp"

namespace nex {

class Application;

/** Модальное окно поверх страницы: заголовок, текст, кнопки (MCU canvas, `sendxy`). */
class MsgBox {
public:
    static constexpr std::size_t kTextCap = 80u;
    static constexpr std::size_t kTitleCap = 32u;

    enum class Preset : uint8_t {
        OK = 0,
        OKCancel,
        YesNo,
        YesNoCancel,
    };
    using Action = msg::evMsgBox::Action;

    static const char* actionCstr(Action action) noexcept;
    static const char* presetCstr(Preset preset) noexcept;
    [[nodiscard]] static Action defaultForPreset(Preset preset) noexcept;

    explicit MsgBox(Application& app) noexcept;

    void show(const char* text, Preset preset = Preset::OK, uint8_t tag = 0u) noexcept;
    void show(const char* text, Preset preset, Action defaultAction, uint8_t tag = 0u) noexcept;

    void show(const char* title, const char* text, Preset preset = Preset::OK, uint8_t tag = 0u) noexcept;
    void show(const char* title, const char* text, Preset preset, Action defaultAction, uint8_t tag = 0u) noexcept;
    
    void show(Preset preset, const char* fmt, ...) noexcept;
    void show(Preset preset, uint8_t tag, const char* fmt, ...) noexcept;
    void show(Preset preset, Action defaultAction, const char* fmt, ...) noexcept;
    void show(Preset preset, Action defaultAction, uint8_t tag, const char* fmt, ...) noexcept;
    
    void show(const char* title, Preset preset, const char* fmt, ...) noexcept;
    void show(const char* title, Preset preset, uint8_t tag, const char* fmt, ...) noexcept;
    void show(const char* title, Preset preset, Action defaultAction, const char* fmt, ...) noexcept;
    void show(const char* title, Preset preset, Action defaultAction, uint8_t tag, const char* fmt, ...) noexcept;

    void showError(Preset preset = Preset::OK, uint8_t tag = msg::evMsgBox::kTagError) noexcept;
    void showError(Preset preset, Action defaultAction, uint8_t tag = msg::evMsgBox::kTagError) noexcept;
    /** Маршрут `evMsgBox` при закрытии; вызвать до `show`, иначе — `currentPageId()` и comp `0`. */
    void setRoute(uint8_t page_id, uint8_t comp_id = 0u) noexcept;
    void onTouchXY(const msg::evTouchXY& e) noexcept;

    [[nodiscard]] bool isActive() const noexcept { return _active; }
    [[nodiscard]] Preset btnPreset() const noexcept { return _preset; }

private:
    /** Рамка MsgBox на экране: геометрия и кнопки. */
    struct Box {
        Region frame;
        Canvas::Button cancelBtn;
        Canvas::Button noBtn;
        Canvas::Button okBtn;
        Canvas::Button yesBtn;

        Box() noexcept;

        void fit(const Rect& screen, Preset preset) noexcept;
        void layoutButtons(Preset preset, Action defaultAction) noexcept;
        void draw(const AppCanvas& cs, const char* title, const char* text, Color titleFg, Action pressedAction,
            bool pressing) const noexcept;

        void hideButtons() noexcept;
        void applyButtonColors(Action defaultAction) noexcept;
        [[nodiscard]] bool hitAction(Point p, Action& out) const noexcept;
        /** Точка `p` внутри кнопки `pressed` (только её region, для click на release). */
        [[nodiscard]] bool hitPressedButton(Point p, Action pressed) const noexcept;
        void drawButton(const AppCanvas& cs, Action action, bool pressing, Action pressedAction) const noexcept;
        void drawButtons(const AppCanvas& cs, bool pressing, Action pressedAction) const noexcept;
    };

    Application& _app;
    Box _box;
    msg::evMsgBox _ev{};
    Preset _preset = Preset::OK;
    Action _defaultAction = Action::Ok;
    bool _active = false;
    bool _pressing = false;
    bool _routePinned = false;

    char _title[kTitleCap]{};
    char _text[kTextCap]{};

    void applyShow(Preset preset, Action defaultAction, uint8_t tag) noexcept;
    void setTitle(const char* title) noexcept;
    void setText(const char* text) noexcept;
    void setTextV(const char* fmt, va_list ap) noexcept;
    void showFormatted(const char* title, Preset preset, Action defaultAction, uint8_t tag, const char* fmt,
        va_list ap) noexcept;
    void present() noexcept;
    /** Press+release на одной кнопке (`sendxy`): обратная связь и закрытие окна. */
    void onButtonClick() noexcept;
};

} // namespace nex
