#pragma once

#include <cstdint>

#include "../core/nexCommands.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexSession.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

/** Зеркала системных переменных NIS и `enqueue` через `Application`. */

class Application;

/** `tag` транзакции / `Application::onResponse` для полей `SysVar` в `Application`. */
enum class SysVarTag : uint8_t {
    Sendxy = 1u,
    Bkcmd,
    Sys0,
    Sys1,
    Sys2,
    Pio0,
    Pio1,
    Pio2,
    Pio3,
    Pio4,
    Pio5,
    Pio6,
    Pio7,
    Tch0,
    Tch1,
    Tch2,
    Tch3,
};

/**
 * Системная переменная NIS (`sys0`, `bkcmd`, …): имя, `tag`, очередь через `Application`.
 * Числовое зеркало и `operator=` — в `SysVar<T>`.
 */
class SysVarBase {
public:
    const Literal name;
    /** Маршрутизация транзакции и `Application::onResponse` (как `tag` у `Attribute`). */
    const uint8_t tag;

    SysVarBase(const SysVarBase&) = delete;
    SysVarBase& operator=(const SysVarBase&) = delete;
    SysVarBase(SysVarBase&&) = delete;
    SysVarBase& operator=(SysVarBase&&) = delete;

    [[nodiscard]] constexpr operator const Literal&() const noexcept { return name; }

    /** `get <sysname>`; ответ — `Application::onResponse(response, Route::sysVar(), tag)`, зеркало — в base. */
    void get() noexcept;

protected:
    explicit SysVarBase(Application& app, const Literal& sysName, uint8_t routeTag) noexcept;

    Application& _app;

    [[nodiscard]] AttrRef target() const noexcept;

    void enqueueTransaction(const Command& cmd, Transaction::Kind kind = Transaction::Kind::Command,
        msg::Status::Mask awaiting_status = msg::kAwaitingAllPanel) const noexcept;
};

/** `sysN=…` через маршрут `Route::kSysVar*` (общий helper для facades / canvas / addons). */
void enqueueSysVarNumericAssign(Application& app, const Literal& sysName, int32_t value) noexcept;

/**
 * Числовая системная переменная: зеркало в MCU, `operator=` → `sys0=…`.
 * Ответ `get` в зеркало — `Application::onResponse` (sysvar route); перехват — override с вызовом base.
 */
template<typename T>
class SysVar : public SysVarBase {
public:
    static_assert(
        wire::is_sysvar_numeric_v<T>,
        "nex::SysVar<T>: T — целое 8/16/32 бит или enum class с underlying uint8_t");

    explicit SysVar(Application& app, const Literal& sysName, SysVarTag routeTag,
        const T& initial = T{}) noexcept
        : SysVarBase(app, sysName, static_cast<uint8_t>(routeTag))
        , _val(initial)
    {}

    SysVar(const SysVar&) = delete;
    SysVar& operator=(const SysVar&) = delete;
    SysVar(SysVar&&) = delete;
    SysVar& operator=(SysVar&&) = delete;

    [[nodiscard]] constexpr operator T() const noexcept { return _val; }

    SysVar& operator=(const T& v) noexcept {
        _val = v;
        const cmd::assign::Numeric cmd(target(), wire::toWire(v));
        enqueueTransaction(cmd, Transaction::Kind::Command, msg::kAwaitingNone);
        return *this;
    }

    void applyResponse(const msg::getNumeric& response) noexcept {
        _val = wire::fromWire<T>(response.value);
    }

private:
    T _val{};
};

} // namespace nex
