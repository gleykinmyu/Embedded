/**
 * @file mech_config.hpp
 * @brief MechConfig — параметры оси (inventory / commissioning).
 *
 * Не runtime IMech и не MotionTarget пресета.
 * Единицы позиции/скорости задаёт kind: Linear → mm, mm/s; Rotary → deg, deg/s
 * (или 0.01° — договориться на leaf). scale: units = counts * num / den;
 * Flag::Invert меняет знак.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bitmask.hpp"

namespace smcp {

/** Тип оси. FlightObject — не вид лебёдки, а объект над осями (отдельно). */
enum class AxisKind : uint8_t {
    Linear = 0, /**< Позиция/лимиты в mm. */
    Rotary = 1, /**< Позиция/лимиты в deg (см. leaf). */
};

/** Поведение тормоза / удержания после остановки. */
enum class BrakePolicy : uint8_t {
    Hold = 0,  /**< Держать позицию / штатный тормоз. */
    Coast = 1, /**< Отпустить после останова (если железо умеет). */
};

enum class MechFlag : uint8_t {
    Invert = 1u << 0,       /**< Знак encoder / «вверх» инвертирован. */
    Continuous = 1u << 1,   /**< Rotary без soft/hard max (оборот). */
    FixedSpeed = 1u << 2,   /**< Только on/off; скорость = vfixed. */
    Mute = 1u << 3,         /**< В inventory, вне спектакля / не в UI. */
    SoftLimits = 1u << 4,   /**< Учитывать soft_min/max. */
    HardLimits = 1u << 5,   /**< Учитывать hard_min/max (концевики/паспорт). */
    LoadSense = 1u << 6,    /**< Есть датчик; смотреть load_*. */
};

/**
 * Конфиг одной оси сегмента (mech_id 0…31).
 * POD для секции inventory / SERV-хвоста; не тащить vptr.
 */
struct MechConfig {
    uint8_t id = 0; /**< mech_id в сегменте. */
    AxisKind kind = AxisKind::Linear;
    REG::BitMask<MechFlag> flag{};
    BrakePolicy brake = BrakePolicy::Hold;

    /** counts * scale_num / scale_den → единицы kind; den == 0 запрещён. */
    int32_t scale_num = 1;
    int32_t scale_den = 1;
    /** Позиция (в единицах kind), соответствующая home/zero encoder. */
    int32_t home = 0;

    int32_t soft_min = 0;
    int32_t soft_max = 0;
    int32_t hard_min = 0;
    int32_t hard_max = 0;

    uint16_t vmax = 0;   /**< Макс. скорость (ед./s). */
    uint16_t vfixed = 0; /**< Скорость при FixedSpeed. */
    uint16_t accel = 0;  /**< Дефолт разгона (ед./s²). */
    uint16_t decel = 0;  /**< Дефолт торможения (ед./s²). */

    int32_t load_min = 0;     /**< Нижнее окно нагрузки (если LoadSense). */
    int32_t load_max = 0;     /**< Верхнее окно. */
    uint32_t load_window = 0; /**< Допуск / чувствительность (ед. датчика). */

    char name[16]{};
    uint8_t reserved_1[4]{};

    [[nodiscard]] constexpr bool isLinear() const noexcept
    {
        return kind == AxisKind::Linear;
    }
    [[nodiscard]] constexpr bool isRotary() const noexcept
    {
        return kind == AxisKind::Rotary;
    }
    [[nodiscard]] constexpr bool isMuted() const noexcept
    {
        return flag.any(MechFlag::Mute);
    }
    [[nodiscard]] constexpr bool isContinuous() const noexcept
    {
        return flag.any(MechFlag::Continuous);
    }
    [[nodiscard]] constexpr bool isFixedSpeed() const noexcept
    {
        return flag.any(MechFlag::FixedSpeed);
    }
    [[nodiscard]] constexpr bool isInverted() const noexcept
    {
        return flag.any(MechFlag::Invert);
    }

    /** soft_max имеет смысл (не continuous). */
    [[nodiscard]] constexpr bool hasSoftMax() const noexcept
    {
        return flag.any(MechFlag::SoftLimits) && !isContinuous();
    }

    void setName(const char* mech_name) noexcept
    {
        if (mech_name == nullptr) {
            name[0] = '\0';
            return;
        }
        std::strncpy(name, mech_name, sizeof(name) - 1u);
        name[sizeof(name) - 1u] = '\0';
    }

    void clear() noexcept
    {
        const uint8_t keep_id = id;
        *this = MechConfig{};
        id = keep_id;
    }
};

static_assert(sizeof(MechConfig) == 72u, "MechConfig wire size");
static_assert(alignof(MechConfig) == alignof(int32_t));
static_assert(std::is_standard_layout_v<MechConfig>);

REG_BITMASK_ENUM_OPS(MechFlag)

} // namespace smcp
