#pragma once

#include "../../comp/nexCanvas.hpp"
#include "../ovlObject.hpp"
#include "../ovlStyle.hpp"

namespace nex::ovl {

struct ButtonStyle {
    TextBoxStyle normal = TextBoxStyle(Color::std::White, Color::std::White, 0u, 1u, 32u, Color::std::Black);
    TextBoxStyle pressed = TextBoxStyle(Color::std::Green, Color::std::White, 0u, 1u, 32u, Color::std::Black);

    constexpr ButtonStyle() noexcept = default;
    constexpr ButtonStyle(TextBoxStyle normal, TextBoxStyle pressed) noexcept : normal(normal), pressed(pressed) {}
};

/** Leaf-кнопка McUI: геометрия в `Object::region()`, draw через `AppCanvas`. */
class Button : public Object {
public:
    Button() noexcept = default;

    Button(const char* label, Rect size, ButtonStyle style = {}) noexcept;

    void setLabel(const char* label) noexcept;
    void setStyle(ButtonStyle style) noexcept;

    void layout() noexcept override;
    void draw(const AppCanvas& cs) const override;

    bool onTouchXY(const msg::evTouchXY& e) noexcept override;

private:
    [[nodiscard]] Rect resolveSize(Rect requested) const noexcept;

    const char* _label = nullptr;
    ButtonStyle _style{};
    bool _pressed = false;
};

} // namespace nex::ovl
