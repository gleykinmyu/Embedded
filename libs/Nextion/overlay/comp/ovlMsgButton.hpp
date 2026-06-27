#pragma once

#include "../../core/nexMessages.hpp"
#include "ovlButton.hpp"

namespace nex::ovl {

struct MsgBoxButtonStyles;

/** Кнопка MsgBox: `Action` + highlight для default-кнопки. */
class MsgButton : public Button {
public:
    using Action = msg::evMsgBox::Action;

    MsgButton(const char* label, Rect size, Action action, ButtonStyle style = {}) noexcept;

    [[nodiscard]] Action action() const noexcept { return _action; }
    void setHighlighted(const MsgBoxButtonStyles& styles, bool on) noexcept;

private:
    Action _action = Action::None;
};

} // namespace nex::ovl
