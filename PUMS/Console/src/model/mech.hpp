/**
 * @file mech.hpp
 * @brief Конкретный IMech пульта: register в smcp::IConsole.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "smcp/Console/console.hpp"
#include "smcp/mech.hpp"
#include "smcp/transport/message.hpp"

/** IMech на пульте: Select TX через IConsole; holder из Telemetry. */
class Mech : public smcp::IMech {
public:
    enum class Type : uint8_t {
        Rope,
        Chain,
    };

    Mech(smcp::IConsole& console, uint8_t id, Type type = Type::Rope) noexcept;

    [[nodiscard]] Type type() const noexcept;
    [[nodiscard]] smcp::IConsole& console() noexcept { return *_console; }
    [[nodiscard]] const smcp::IConsole& console() const noexcept { return *_console; }

    void select(uint8_t console_id) noexcept override;
    void block(bool blocked) noexcept override;
    void setTarget(const smcp::MotionTarget& target) noexcept override;
    void resetFault() noexcept override;

    void onTelemetry(uint8_t src_id, const smcp::msg::Telemetry& telemetry) noexcept;

private:
    smcp::IConsole* _console;
    Type _type;
};

/** Банк Mech[N]: ctor регистрирует каждый в owner. */
template <std::size_t N>
class MechBank {
public:
    explicit MechBank(smcp::IConsole& owner) noexcept
        : MechBank(owner, std::make_index_sequence<N>{})
    {}

    [[nodiscard]] Mech& operator[](std::size_t i) noexcept { return _items[i]; }
    [[nodiscard]] const Mech& operator[](std::size_t i) const noexcept { return _items[i]; }

    [[nodiscard]] Mech* begin() noexcept { return _items; }
    [[nodiscard]] Mech* end() noexcept { return _items + N; }
    [[nodiscard]] const Mech* begin() const noexcept { return _items; }
    [[nodiscard]] const Mech* end() const noexcept { return _items + N; }

private:
    template <std::size_t... I>
    MechBank(smcp::IConsole& owner, std::index_sequence<I...>) noexcept
        : _items{Mech{owner, static_cast<uint8_t>(I)}...}
    {}

    Mech _items[N];
};
