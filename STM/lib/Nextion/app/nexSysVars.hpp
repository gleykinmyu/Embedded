#pragma once

#include <cstdint>
#include <type_traits>

#include "../core/nexCommands.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexSession.hpp"

namespace nex {

class Application;

namespace sysvar_detail {

template<typename T>
inline constexpr bool is_nex_numeric_storage_v = std::disjunction_v<
    std::is_convertible<T, bool>,
    std::is_convertible<T, int8_t>,
    std::is_convertible<T, uint8_t>,
    std::is_convertible<T, int16_t>,
    std::is_convertible<T, uint16_t>,
    std::is_convertible<T, int32_t>>;

template<typename T, bool IsEnum = std::is_enum_v<T>>
struct is_nex_enum_storage : std::false_type {};

template<typename T>
struct is_nex_enum_storage<T, true> : std::is_same<std::underlying_type_t<T>, uint8_t> {};

template<typename T>
inline constexpr bool is_nex_enum_storage_v = is_nex_enum_storage<T>::value;

template<typename T>
[[nodiscard]] constexpr int32_t to_wire(const T& v) noexcept {
    if constexpr (is_nex_enum_storage_v<T>)
        return static_cast<int32_t>(static_cast<uint8_t>(v));
    return static_cast<int32_t>(v);
}

template<typename T>
[[nodiscard]] constexpr T from_wire(int32_t wire) noexcept {
    if constexpr (is_nex_enum_storage_v<T>)
        return static_cast<T>(static_cast<uint8_t>(wire));
    return static_cast<T>(wire);
}

} // namespace sysvar_detail

/** `tag` транзакции / `onSysResponse` для полей `SysVar` в `Application`. */
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

/** NIS §6 `bkcmd`: pass/fail после команд по UART (0–3, на панели default 2). */
enum class BkCmd : uint8_t {
    Off = 0,
    OnSuccess = 1,
    OnFailure = 2,
    Always = 3,
};

/**
 * Системная переменная NIS (`sys0`, `bkcmd`, …): имя, `tag`, очередь через `Application`.
 * Числовое зеркало и `operator=` — в `SysVar<T>`.
 */
class SysVarBase {
public:
    const Literal name;
    /** Маршрутизация транзакции и `onSysResponse` (как `tag` у `Attribute`). */
    const uint8_t tag;

    SysVarBase(const SysVarBase&) = delete;
    SysVarBase& operator=(const SysVarBase&) = delete;
    SysVarBase(SysVarBase&&) = delete;
    SysVarBase& operator=(SysVarBase&&) = delete;

    [[nodiscard]] constexpr operator const Literal&() const noexcept { return name; }

    /** `get <sysname>`; ответ — `Application::onSysResponse(tag, …)`, зеркало — `SysVar::applyResponse`. */
    void get() noexcept;

protected:
    explicit SysVarBase(Application& app, const Literal& sysName, uint8_t routeTag) noexcept;

    Application& _app;

    [[nodiscard]] AttrRef target() const noexcept;

    void enqueueTransaction(const Command& cmd,
        Transaction::State state = Transaction::State::AwaitingStatus) const noexcept;
};

/** `sysN=…` через маршрут `Route::kSysVar*` (общий helper для facades / canvas / addons). */
void enqueueSysVarNumericAssign(Application& app, const Literal& sysName, int32_t value) noexcept;

/**
 * Числовая системная переменная: зеркало в MCU, `operator=` → `sys0=…`.
 * Ответ `get` в зеркало — вручную: `onSysResponse` → `applyResponse`.
 */
template<typename T>
class SysVar : public SysVarBase {
public:
    static_assert(
        sysvar_detail::is_nex_numeric_storage_v<T> || sysvar_detail::is_nex_enum_storage_v<T>,
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
        const cmd::assign::Numeric cmd(target(), sysvar_detail::to_wire(v));
        enqueueTransaction(cmd);
        return *this;
    }

    void applyResponse(const msg::getNumeric& response) noexcept {
        _val = sysvar_detail::from_wire<T>(response.value);
    }

private:
    T _val{};
};

} // namespace nex
