/**
 * @file mech.hpp
 * @brief Mech консоли: Select/SetTarget/… → static owner (MConsole).
 */

#pragma once

#include <cstdint>

#include "smcp/mech.hpp"
#include "smcp/message.hpp"

class MConsole;

/** Реализация IMech на пульте: запросы через static owner; UI — через MConsole. */
class Mech : public smcp::IMech {
public:
    enum class Type : uint8_t {
        Rope,
        Chain,
    };

    Mech() noexcept;
    Mech(uint8_t id, Type type = Type::Rope) noexcept;

    [[nodiscard]] Type type() const noexcept;

    static void setOwner(MConsole* owner) noexcept { s_owner = owner; }
    [[nodiscard]] static MConsole* owner() noexcept { return s_owner; }

    /**
     * console_id != 0 → Select; 0 → Deselect.
     * При установленном owner — только TX (Selected из Telemetry).
     */
    bool select(uint8_t console_id) noexcept override;
    bool block(bool blocked) noexcept override;
    bool setTarget(const smcp::MotionTarget& target) noexcept override;
    bool resetFault() noexcept override;

    /** Применить телеметрию; show-Blocked на консоли не снимается. */
    void onTelemetry(uint8_t src_id, const smcp::msg::Telemetry& telemetry) noexcept override;

private:
    Type _type;
    static MConsole* s_owner;
};
