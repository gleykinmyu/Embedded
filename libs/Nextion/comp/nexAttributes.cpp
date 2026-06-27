#include "nexAttributes.hpp"

namespace nex {

void attr::Base::pushCmdAssignText(const char* text, cmd::assign::Text::Op op) const noexcept {
    const AttrRef target{ _parent.name, name() };
    const char* const p = text != nullptr ? text : "";
    const cmd::assign::Text cmd(target, p, op);
    enqueueTransaction(cmd, Transaction::Kind::Command, msg::kAwaitingNone);
}

void attr::Base::pushCmdAssignTextSubtract(uint32_t n) const noexcept {
    const AttrRef target{ _parent.name, name() };
    const cmd::assign::TextSubtract cmd(target, n);
    enqueueTransaction(cmd, Transaction::Kind::Command, msg::kAwaitingNone);
}

} // namespace nex
