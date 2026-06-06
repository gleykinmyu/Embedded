#include "nexSysVars.hpp"
#include "nexApplication.hpp"

namespace nex {

SysVarBase::SysVarBase(Application& app, const Literal& sysName, uint8_t routeTag) noexcept
    : name(sysName)
    , tag(routeTag)
    , _app(app)
{}

void SysVarBase::get() noexcept {
    enqueueTransaction(cmd::Get::numeric(target()), Transaction::State::AwaitingNumericGet);
}

AttrRef SysVarBase::target() const noexcept {
    return AttrRef{kEmptyLiteral, name};
}

void SysVarBase::enqueueTransaction(const Command& cmd, Transaction::State state) const noexcept {
    _app.enqueue(Transaction{cmd, Route::kSysVarPageId, Route::kSysVarCompId, tag, state});
}

void enqueueSysVarNumericAssign(Application& app, const Literal& sysName, int32_t value) noexcept {
    const AttrRef target{kEmptyLiteral, sysName};
    app.enqueue(Transaction{cmd::assign::Numeric(target, value), Route::kSysVarPageId, Route::kSysVarCompId, 0u});
}

} // namespace nex
