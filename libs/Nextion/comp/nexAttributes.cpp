#include "nexAttributes.hpp"

namespace nex {

void attr::Base::pushCmdAssignText(const char* text, cmd::assign::Text::Op op) const noexcept
{
    if (!_parent.canAccess()) {
        return;
    }
    const char* const p = text != nullptr ? text : "";
    enqueueTransaction(cmd::assign::Text(attr_detail::makeTarget(_parent, id), p, op),
        Transaction::Kind::Command, msg::kAwaitingNone);
}

void attr::Base::pushCmdAssignTextSubtract(uint32_t n) const noexcept
{
    if (!_parent.canAccess()) {
        return;
    }
    enqueueTransaction(cmd::assign::TextSubtract(attr_detail::makeTarget(_parent, id), n),
        Transaction::Kind::Command, msg::kAwaitingNone);
}

} // namespace nex
