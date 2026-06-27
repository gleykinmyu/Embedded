#include "ovlMsgBox.hpp"

namespace nex::ovl {

MsgButton::MsgButton(const char* const label, const Rect size, const Action action, const ButtonStyle style) noexcept
    : Button(label, size, style), _action(action) {}

void MsgButton::setHighlighted(const MsgBoxButtonStyles& styles, const bool on) noexcept {
    setStyle(on ? styles.highlighted : styles.normal);
}

} // namespace nex::ovl
