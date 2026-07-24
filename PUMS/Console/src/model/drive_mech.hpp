/**
 * @file drive_mech.hpp
 * @brief Локальный механизм сегмента (MServer): select применяет сразу, без TX.
 */

#pragma once

#include <cstdint>

#include "smcp/mech.hpp"

/** IMech на стороне сервера: лимит Selected, состояние до Telemetry наружу. */
class DriveMech : public smcp::IMech {
public:
    static constexpr uint8_t kMaxSelected = 3u;

    enum class Type : uint8_t {
        Rope,
        Chain,
    };

    DriveMech() noexcept;
    DriveMech(uint8_t id, Type type = Type::Rope) noexcept;

    [[nodiscard]] Type type() const noexcept { return _type; }

    bool select(uint8_t console_id) noexcept override;
    bool block(bool blocked) noexcept override;
    bool setTarget(const smcp::MotionTarget& target) noexcept override;
    bool resetFault() noexcept override;

private:
    Type _type = Type::Rope;
    static uint8_t s_selectedCount;
};
