/**
 * @file cmech.hpp
 * @brief CMech — IMech пульта: Select/Block/SetTarget TX через IConsole, Telemetry RX.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "smcp/mech.hpp"
#include "smcp/transport/message.hpp"

namespace smcp {

class IConsole;

/** IMech на пульте: Select/Block/SetTarget — TX; holder/status — только Telemetry. */
class CMech : public IMech {
public:
    CMech(IConsole& console, uint8_t id) noexcept;

    [[nodiscard]] IConsole& console() noexcept { return *_console; }
    [[nodiscard]] const IConsole& console() const noexcept { return *_console; }

    void select(uint8_t console_id) noexcept override;
    /** Сегментный Block на сервер (не GRUP / не локальный Status). */
    void block(bool blocked) noexcept override;
    void setTarget(const MotionTarget& target) noexcept override;
    void resetFault() noexcept override;

    /** Зеркало Telemetry (holder / status / position). */
    void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

private:
    IConsole* _console;
};

/** Банк CMech[N]: ctor регистрирует каждый в owner. */
template <uint8_t N>
class CMechBank {
public:
    explicit CMechBank(IConsole& owner) noexcept
        : CMechBank(owner, std::make_index_sequence<N>{})
    {}

    [[nodiscard]] CMech& operator[](uint8_t i) noexcept { return _items[i]; }
    [[nodiscard]] const CMech& operator[](uint8_t i) const noexcept { return _items[i]; }

    [[nodiscard]] CMech* begin() noexcept { return _items; }
    [[nodiscard]] CMech* end() noexcept { return _items + N; }
    [[nodiscard]] const CMech* begin() const noexcept { return _items; }
    [[nodiscard]] const CMech* end() const noexcept { return _items + N; }

private:
    template <std::size_t... I>
    CMechBank(IConsole& owner, std::index_sequence<I...>) noexcept
        : _items{CMech{owner, static_cast<uint8_t>(I)}...}
    {}

    CMech _items[N];
};

} // namespace smcp
