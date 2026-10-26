/**
 * @file show_axis.hpp
 * @brief Шоу-ось: ShowType (библиотека) + Patch (экземпляр ATYP/APCH).
 *
 * Profile железа — smcp/mech/axis.hpp (RAM сервера, не в шоуфайле).
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "bitmask.hpp"
#include "enum_value.hpp"
#include "smcp/mech/axis.hpp"

namespace smcp {
namespace show {

/** Patch.id == 0 — пустой экземпляр; type_id == kTypeNone — без типа. */
inline constexpr uint16_t kIdNone = 0u;
inline constexpr uint8_t kTypeNone = 0xFFu;

/** Флаги привязки (Patch). Invert только здесь. */
enum class PatchFlag : uint8_t {
    Invert = 1u << 0, /**< «Вверх» шоу противоположен направлению привода. */
    All = Invert,
};

REG_BITMASK_ENUM_OPS(PatchFlag)

/**
 * Тип оси спектакля (библиотека; секция Types / ATYP).
 * Soft / scale / kind — набор характеристик; имя — типа («Штанкет 500»), не экземпляра.
 * Пустой слот — name[0] == 0. Ссылка из Patch.type_id (индекс).
 */
struct alignas(uint32_t) ShowType {
    MISC::EnumValue<axis::Kind> kind{axis::Kind::Linear};
    MISC::EnumValue<axis::UnitScale> scale{axis::UnitScale::E3}; /**< Шкала консоли. */
    uint8_t reserved_0[2]{};

    /** Soft / потолки спектакля (ед. консоли); 0 у max — нет ограничения шоу. */
    struct {
        axis::Range<int32_t> pos{};       /**< Soft позиции; min==max==0 — нет. */
        axis::RangeMax<uint16_t> speed{}; /**< Потолок скорости. */
        axis::RangeMax<uint16_t> accel{}; /**< Потолок ускорения. */
    } limits{};

    char name[axis::kNameSize]{}; /**< Имя типа. */

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return name[0] == '\0'; }

    void setName(const char* type_name) noexcept { axis::detail::copyName(name, type_name); }

    void clear() noexcept { *this = ShowType{}; }
};

static_assert(sizeof(ShowType) == 32u, "show::ShowType wire size");
static_assert(alignof(ShowType) == alignof(uint32_t));
static_assert(std::is_standard_layout_v<ShowType>);

/**
 * Экземпляр оси спектакля (секция Patch / APCH): show-имя, логический id, тип, Route.
 * id == 0 — пустой слот. route.empty() — ещё не пропатчен.
 * hard / vmax — из axis::Profile (RAM сервера), не здесь.
 */
struct alignas(uint32_t) Patch {
    uint16_t id = kIdNone; /**< Логический id (любой ≠ 0; пропуски допустимы). */
    uint8_t type_id = kTypeNone; /**< Индекс в библиотеке ShowType. */
    axis::Route route{};         /**< server_id + axis_id; empty — не пропатчен. */
    REG::BitMask<PatchFlag> flag{};
    uint8_t reserved_0[2]{};

    int32_t pos_offset = 0; /**< Trim спектакля в ед. консоли оси. */

    char name[axis::kNameSize]{}; /**< Show-имя («Портал L»). */

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return id == kIdNone; }
    [[nodiscard]] constexpr bool isPatched() const noexcept { return !route.empty(); }

    void setName(const char* show_name) noexcept { axis::detail::copyName(name, show_name); }

    void clear() noexcept { *this = Patch{}; }
};

static_assert(sizeof(Patch) == 28u, "show::Patch wire size");
static_assert(alignof(Patch) == alignof(uint32_t));
static_assert(std::is_standard_layout_v<Patch>);

} // namespace show
} // namespace smcp
