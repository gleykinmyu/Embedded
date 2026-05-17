#pragma once

#include <cstddef>
#include <cstdint>

#include "../core/nexTypes.hpp"

namespace nex {

class Application;

namespace msg {
    struct evTouchXY;
}

/** Модальное окно поверх страницы: текст + кнопка OK (MCU canvas, `sendxy`). */
class MsgBox {
public:
    static constexpr std::size_t kTextCap = 80u;

    explicit MsgBox(Application& app) noexcept;

    void show(const char* text) noexcept;
    void showError() noexcept;
    void onTouchXY(const msg::evTouchXY& e) noexcept;

    [[nodiscard]] bool isActive() const noexcept { return _active; }

private:
    Application& _app;
    bool _active = false;
    bool _okPressed = false;
    bool _isError = false;
    char _text[kTextCap]{};
    Point _boxUl{};
    Point _boxLr{};
    Point _okUl{};
    Point _okLr{};

    void setText(const char* text) noexcept;
    void present() noexcept;
    void dismiss() noexcept;
    void draw() noexcept;
    void drawOkButton(bool pressed) noexcept;
    [[nodiscard]] static bool contains(Point p, Point ul, Point lr) noexcept;
    [[nodiscard]] bool okContains(Point p) const noexcept;
};

} // namespace nex
