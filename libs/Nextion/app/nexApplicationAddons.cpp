// Команды shell и системные переменные `Application` — вынесены из `nexApplication.cpp`.

#include "nexApplication.hpp"
#include "nexSysVars.hpp"

namespace nex {

namespace {

constexpr Literal kSysBaud{"baud"};
constexpr Literal kSysBauds{"bauds"};
constexpr Literal kSysAddr{"addr"};
constexpr Literal kSysDim{"dim"};
constexpr Literal kSysDims{"dims"};

} // namespace

void Application::switchPage(uint8_t pageId) noexcept {
    enqueue(Transaction{cmd::Page::switchTo(pageId), 0u, 0u, 0u, Transaction::Kind::Command, msg::kAwaitingPageCommand});
}

void Application::switchPage(const Literal& pageName) noexcept {
    enqueue(Transaction{cmd::Page::switchTo(pageName), 0u, 0u, 0u, Transaction::Kind::Command, msg::kAwaitingPageCommand});
}

void Application::requestCurrentPage() noexcept {
    enqueue(Transaction{cmd::Page::sendMe(), 0u, 0u, 0u, Transaction::Kind::Command, msg::kAwaitingPageCommand});
}

void Application::refreshPage() noexcept {
    enqueue(Transaction{cmd::Page::refresh(), 0u, 0u, 0u, Transaction::Kind::Command, msg::kAwaitingPageCommand});
}

void Application::restartScreen() noexcept {
    enqueue(Transaction{cmd::System::restart(), 0u, 0u});
}

void Application::setBaudrate(Baudrate rate) noexcept {
    enqueueSysVarNumericAssign(*this, kSysBaud, static_cast<int32_t>(rate));
}

void Application::setBaudrateDefault(Baudrate rate) noexcept {
    enqueueSysVarNumericAssign(*this, kSysBauds, static_cast<int32_t>(rate));
}

void Application::setAddress(uint16_t address) noexcept {
    enqueueSysVarNumericAssign(*this, kSysAddr, static_cast<int32_t>(address));
}

void Application::setBrightness(uint8_t level) noexcept {
    enqueueSysVarNumericAssign(*this, kSysDim, static_cast<int32_t>(level));
}

void Application::setBrightnessDefault(uint8_t level) noexcept {
    enqueueSysVarNumericAssign(*this, kSysDims, static_cast<int32_t>(level));
}

void Application::setGlobalVar(const Literal& path, int32_t value) noexcept {
    const AttrRef target{kEmptyLiteral, path};
    enqueue(Transaction{cmd::assign::Numeric(target, value), Route{}, 0u, Transaction::Kind::Command,
        msg::kAwaitingNone});
}

} // namespace nex
