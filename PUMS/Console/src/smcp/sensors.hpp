/**
 * @file sensors.hpp
 * @brief Датчики, концевики и реакции системы SMCP v1.5 (блок 2.2).
 */

#pragma once

#include <cstdint>

#include "fsm.hpp"

namespace smcp {

/** Категория датчика по уровню критичности. */
enum class SensorKind : uint8_t {
    WorkLimitUp,
    WorkLimitDown,
    Overtravel,
    SlackWire,
    Overload,
};

/** Реакция системы на срабатывание датчика. */
enum class SensorReaction : uint8_t {
    SmoothStop,
    ImmediateFault,
};

/** Допустимое направление движения после срабатывания (для рабочих лимитов). */
enum class AllowedMotion : uint8_t {
    None,
    ReverseOnly,
    UpOnly,
};

struct SensorPolicy {
    SensorKind kind;
    SensorReaction reaction;
    MechState target_state;
    AllowedMotion allowed_motion;
};

inline constexpr SensorPolicy kSensorPolicies[] = {
    {SensorKind::WorkLimitUp, SensorReaction::SmoothStop, MechState::Stopping, AllowedMotion::ReverseOnly},
    {SensorKind::WorkLimitDown, SensorReaction::SmoothStop, MechState::Stopping, AllowedMotion::ReverseOnly},
    {SensorKind::Overtravel, SensorReaction::ImmediateFault, MechState::Fault, AllowedMotion::None},
    {SensorKind::SlackWire, SensorReaction::ImmediateFault, MechState::Fault, AllowedMotion::UpOnly},
    {SensorKind::Overload, SensorReaction::ImmediateFault, MechState::Fault, AllowedMotion::None},
};

[[nodiscard]] constexpr SensorReaction reactionFor(SensorKind kind) noexcept
{
    for (const auto& policy : kSensorPolicies) {
        if (policy.kind == kind) {
            return policy.reaction;
        }
    }
    return SensorReaction::ImmediateFault;
}

} // namespace smcp
