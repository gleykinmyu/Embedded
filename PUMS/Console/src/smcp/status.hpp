/**
 * @file status.hpp
 * @brief Битовые маски статуса и опций механизма SMCP v1.5.
 */

#pragma once

#include <cstdint>

namespace smcp {

/** Телеметрическая маска оси (блок 1.6). */
enum StatusMask : uint8_t {
    StatusMoving = 1u << 0,
    StatusArmed = 1u << 1,
    StatusTarget = 1u << 2,
    StatusFault = 1u << 3,
    StatusLeased = 1u << 4,
    StatusLimit1 = 1u << 5,
    StatusLimit2 = 1u << 6,
    StatusInhibit = 1u << 7,
};

/** Биты поля Options в MechConfig (блок 3.6). */
enum OptionsMask : uint8_t {
    OptInvEnc = 1u << 0,
    OptInvDir = 1u << 1,
    OptNoBrake = 1u << 2,
    OptRotary = 1u << 3,
};

/**
 * Семейство механизма по старшей тетраде Type (блок 3.2).
 * Младшая тетрада — подтип внутри семейства.
 */
enum MechFamily : uint8_t {
    FamilyVertical = 0x10,
    FamilyHorizontal = 0x20,
    FamilyRotational = 0x30,
    FamilyPoint = 0x40,
};

[[nodiscard]] constexpr uint8_t mechFamily(uint8_t type) noexcept
{
    return static_cast<uint8_t>(type & 0xF0u);
}

[[nodiscard]] constexpr uint8_t mechSubtype(uint8_t type) noexcept
{
    return static_cast<uint8_t>(type & 0x0Fu);
}

[[nodiscard]] constexpr bool hasStatus(uint8_t mask, StatusMask bit) noexcept
{
    return (mask & static_cast<uint8_t>(bit)) != 0u;
}

[[nodiscard]] constexpr bool hasOption(uint8_t options, OptionsMask bit) noexcept
{
    return (options & static_cast<uint8_t>(bit)) != 0u;
}

} // namespace smcp
