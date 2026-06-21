#include "nexSysVars.hpp"
#include "nexApplication.hpp"

namespace nex {

SysVarBase::SysVarBase(Application& app, const Literal& sysName, uint8_t routeTag) noexcept
    : name(sysName)
    , tag(routeTag)
    , _app(app)
{}

void SysVarBase::get() noexcept {
    enqueueTransaction(cmd::Get::numeric(target()), Transaction::Kind::GetNumeric);
}

AttrRef SysVarBase::target() const noexcept {
    return AttrRef{kEmptyLiteral, name};
}

void SysVarBase::enqueueTransaction(
    const Command& cmd, Transaction::Kind kind, msg::Status::Mask awaiting_status) const noexcept {
    _app.enqueue(
        Transaction{cmd, Route::kSysVarPageId, Route::kSysVarCompId, tag, kind, awaiting_status});
}

void enqueueSysVarNumericAssign(Application& app, const Literal& sysName, int32_t value) noexcept {
    const AttrRef target{kEmptyLiteral, sysName};
    app.enqueue(Transaction{cmd::assign::Numeric(target, value), Route::kSysVarPageId, Route::kSysVarCompId, 0u,
        Transaction::Kind::Command, msg::kAwaitingNone});
}

} // namespace nex
