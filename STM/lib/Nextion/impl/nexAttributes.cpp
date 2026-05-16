#include "nexAttributes.hpp"

namespace nex {

void Attribute::pushCmdAssignText(const char* text, cmd::assign::Text::Op op) const noexcept {
    const cmd::TargetAttr target{ _parent.name, name };
    const char* const p = text != nullptr ? text : "";
    const cmd::assign::Text cmd(target, p, op);
    enqueueTransaction(cmd);
}

void Attribute::pushCmdAssignTextSubtract(uint32_t n) const noexcept {
    const cmd::TargetAttr target{ _parent.name, name };
    const cmd::assign::TextSubtract cmd(target, n);
    enqueueTransaction(cmd);
}

} // namespace nex
