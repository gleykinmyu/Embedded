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
    return AttrRef{kEmptyCompLexeme, name};
}

void SysVarBase::enqueueTransaction(const Command& cmd, Transaction::State state) const noexcept {
    _app.enqueue(Transaction{cmd, Application::kSysVarRoutePageId, Application::kSysVarRouteCompId, tag, state});
}

} // namespace nex
