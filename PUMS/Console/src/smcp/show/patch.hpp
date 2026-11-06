/**
 * @file patch.hpp
 * @brief smcp::show — AxisType / PatchRec + TypeSection (ATYP) + PatchSection (APCH) + IPatch / Patch.
 *
 * Оба required. PatchRec.type_id → индекс в ATYP (kTypeNone — без типа).
 * Пропуски PatchRec.id допустимы. Profile железа в файл не входит (RAM сервера).
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bitmask.hpp"
#include "enum_value.hpp"
#include "segment/axis.hpp"
#include "showFile.hpp"
#include "tags.hpp"

namespace smcp {
namespace show {

/** PatchRec.id == 0 — пустой экземпляр; type_id == kTypeNone — без типа. */
inline constexpr uint16_t kIdNone = 0u;
inline constexpr uint8_t kTypeNone = 0xFFu;

/** FourCC: "ATYP" / "APCH". */
inline constexpr sf::Tag kAxisTypeTag = 0x50595441u;
inline constexpr sf::Tag kPatchTag = 0x48435041u;

/** Ёмкость библиотеки типов и записей APCH (пропуски PatchRec.id допустимы). */
inline constexpr uint8_t kAxisTypeCount = 16u;
inline constexpr uint8_t kPatchCount = 32u;

/** Флаги привязки (PatchRec). Invert только здесь. */
enum class PatchFlag : uint8_t {
    Invert = 1u << 0, /**< «Вверх» шоу противоположен направлению привода. */
    All = Invert,
};

REG_BITMASK_ENUM_OPS(PatchFlag)

/**
 * Тип оси спектакля (библиотека; секция ATYP).
 * Soft / scale / kind — набор характеристик; имя — типа («Штанкет 500»), не экземпляра.
 * Пустой слот — name[0] == 0. Ссылка из PatchRec.type_id (индекс).
 */
struct alignas(uint32_t) AxisType {
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

    void clear() noexcept { *this = AxisType{}; }
};

static_assert(sizeof(AxisType) == 32u, "show::AxisType wire size");
static_assert(alignof(AxisType) == alignof(uint32_t));
static_assert(std::is_standard_layout_v<AxisType>);

/**
 * Запись APCH: show-имя, логический id, тип, Route.
 * id == 0 — пустой слот. route.empty() — без адреса на шине (!isRouted).
 * hard / vmax — из axis::Profile (RAM сервера), не здесь.
 */
struct alignas(uint32_t) PatchRec {
    uint16_t id = kIdNone; /**< Логический id (любой ≠ 0; пропуски допустимы). */
    uint8_t type_id = kTypeNone; /**< Индекс в библиотеке AxisType. */
    axis::Route route{};         /**< server_id + axis_id; empty — !isRouted(). */
    REG::BitMask<PatchFlag> flag{};
    uint8_t reserved_0[2]{};

    int32_t pos_offset = 0; /**< Trim спектакля в ед. консоли оси. */

    char name[axis::kNameSize]{}; /**< Show-имя («Портал L»). */

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return id == kIdNone; }
    [[nodiscard]] constexpr bool isRouted() const noexcept { return !route.empty(); }

    void setName(const char* show_name) noexcept { axis::detail::copyName(name, show_name); }

    void clear() noexcept { *this = PatchRec{}; }
};

static_assert(sizeof(PatchRec) == 28u, "show::PatchRec wire size");
static_assert(alignof(PatchRec) == alignof(uint32_t));
static_assert(std::is_standard_layout_v<PatchRec>);

/** Секция ATYP — библиотека AxisType. */
template <uint8_t NType = kAxisTypeCount>
class TypeSection : public sf::Section<AxisType, NType> {
    static_assert(NType > 0u && NType < kTypeNone, "TypeSection: NType fits type_id");
    using Base = sf::Section<AxisType, NType>;

public:
    static constexpr uint8_t kCount = NType;

    explicit TypeSection(sf::IShow& file, bool required = true) noexcept
        : Base(file, kAxisTypeTag, required)
    {}

    [[nodiscard]] bool isValid() const noexcept override
    {
        for (uint8_t i = 0; i < NType; ++i) {
            const AxisType& t = this->rec(i);
            if (t.isEmpty()) {
                continue;
            }
            if (std::memchr(t.name, '\0', axis::kNameSize) == nullptr) {
                return false;
            }
            if (!t.kind.isValid() || !t.scale.isValid()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] AxisType* get(uint8_t i) noexcept
    {
        return i < NType ? &this->rec(i) : nullptr;
    }
    [[nodiscard]] const AxisType* get(uint8_t i) const noexcept
    {
        return i < NType ? &this->rec(i) : nullptr;
    }
};

/**
 * Секция APCH — записи PatchRec.
 * Держит ссылку на TypeSection для isValid (type_id → непустой тип).
 */
template <uint8_t NPatch = kPatchCount, uint8_t NType = kAxisTypeCount>
class PatchSection : public sf::Section<PatchRec, NPatch> {
    static_assert(NPatch > 0u, "PatchSection: NPatch > 0");
    static_assert(NType > 0u && NType < kTypeNone, "PatchSection: NType fits type_id");
    using Base = sf::Section<PatchRec, NPatch>;

public:
    static constexpr uint8_t kCount = NPatch;

    PatchSection(sf::IShow& file, TypeSection<NType>& types, bool required = true) noexcept
        : Base(file, kPatchTag, required)
        , _types(types)
    {}

    [[nodiscard]] bool isValid() const noexcept override
    {
        constexpr uint8_t kKnown = static_cast<uint8_t>(PatchFlag::All);
        for (uint8_t i = 0; i < NPatch; ++i) {
            const PatchRec& p = this->rec(i);
            if (p.isEmpty()) {
                continue;
            }
            if ((p.flag.raw() & static_cast<uint8_t>(~kKnown)) != 0u) {
                return false;
            }
            if (std::memchr(p.name, '\0', axis::kNameSize) == nullptr) {
                return false;
            }
            if (p.type_id != kTypeNone) {
                const AxisType* t = _types.get(p.type_id);
                if (t == nullptr || t->isEmpty()) {
                    return false;
                }
            }
            if (!p.route.isValid()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] PatchRec* get(uint8_t i) noexcept
    {
        return i < NPatch ? &this->rec(i) : nullptr;
    }
    [[nodiscard]] const PatchRec* get(uint8_t i) const noexcept
    {
        return i < NPatch ? &this->rec(i) : nullptr;
    }

    /** Запись по логическому id; нет — nullptr (пропуски допустимы). */
    [[nodiscard]] PatchRec* byId(uint16_t id) noexcept
    {
        if (id == kIdNone) {
            return nullptr;
        }
        for (uint8_t i = 0; i < NPatch; ++i) {
            PatchRec& p = this->rec(i);
            if (p.id == id) {
                return &p;
            }
        }
        return nullptr;
    }

private:
    TypeSection<NType>& _types;
};

/**
 * Бесшаблонный патч: две секции ATYP / APCH (payload = AxisType[] / PatchRec[]).
 * Ссылки type_id → непустой тип проверяет PatchSection::isValid.
 * Секции — _types/_recs (не public types/recs: иначе конфликт имён с Patch::types).
 */
class IPatch {
public:
    IPatch(const IPatch&) = delete;
    IPatch& operator=(const IPatch&) = delete;

    template <uint8_t NType, uint8_t NPatch>
    explicit IPatch(TypeSection<NType>& types, PatchSection<NPatch, NType>& recs) noexcept
        : _types(types)
        , _recs(recs)
    {}

    [[nodiscard]] sf::ISection& types() noexcept { return _types; }
    [[nodiscard]] const sf::ISection& types() const noexcept { return _types; }
    [[nodiscard]] sf::ISection& recs() noexcept { return _recs; }
    [[nodiscard]] const sf::ISection& recs() const noexcept { return _recs; }

    [[nodiscard]] uint8_t typeCount() const noexcept
    {
        return static_cast<uint8_t>(_types.slotCount());
    }
    [[nodiscard]] uint8_t axisCount() const noexcept
    {
        return static_cast<uint8_t>(_recs.slotCount());
    }

    [[nodiscard]] AxisType* type(uint8_t i) noexcept
    {
        return const_cast<AxisType*>(static_cast<const IPatch*>(this)->type(i));
    }
    [[nodiscard]] const AxisType* type(uint8_t i) const noexcept
    {
        if (i >= _types.slotCount()) {
            return nullptr;
        }
        return reinterpret_cast<const AxisType*>(_types.payload()) + i;
    }

    [[nodiscard]] PatchRec* rec(uint8_t i) noexcept
    {
        return const_cast<PatchRec*>(static_cast<const IPatch*>(this)->rec(i));
    }
    [[nodiscard]] const PatchRec* rec(uint8_t i) const noexcept
    {
        if (i >= _recs.slotCount()) {
            return nullptr;
        }
        return reinterpret_cast<const PatchRec*>(_recs.payload()) + i;
    }

    [[nodiscard]] AxisType* typeOf(const PatchRec& p) noexcept
    {
        return const_cast<AxisType*>(static_cast<const IPatch*>(this)->typeOf(p));
    }
    [[nodiscard]] const AxisType* typeOf(const PatchRec& p) const noexcept
    {
        return p.type_id == kTypeNone ? nullptr : type(p.type_id);
    }

    /** Есть ли непустая запись с этим type_id. */
    [[nodiscard]] bool isUsedType(uint8_t type_id) const noexcept
    {
        if (type_id == kTypeNone || type_id >= typeCount()) {
            return false;
        }
        const uint8_t n = axisCount();
        for (uint8_t i = 0; i < n; ++i) {
            const PatchRec* p = rec(i);
            if (p != nullptr && !p->isEmpty() && p->type_id == type_id) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return _types.isValid() && _recs.isValid();
    }

private:
    sf::ISection& _types; /**< ATYP, записи AxisType. */
    sf::ISection& _recs;  /**< APCH, записи PatchRec. */
};

namespace detail {

/** Секции до IPatch: порядок баз Patch = sections → IPatch(types, recs). */
template <uint8_t NType, uint8_t NPatch>
struct PatchSections {
    TypeSection<NType> types;
    PatchSection<NPatch, NType> recs;

    explicit PatchSections(sf::IShow& file, bool required) noexcept
        : types(file, required)
        , recs(file, types, required)
    {}
};

} // namespace detail

/**
 * Патч спектакля: TypeSection + PatchSection, is-a IPatch (не sf::SectionPool).
 */
template <uint8_t NType = kAxisTypeCount, uint8_t NPatch = kPatchCount>
class Patch : public detail::PatchSections<NType, NPatch>, public IPatch {
public:
    static constexpr uint8_t kTypeCount = NType;
    static constexpr uint8_t kInstCount = NPatch;

    using detail::PatchSections<NType, NPatch>::types;
    using detail::PatchSections<NType, NPatch>::recs;

    explicit Patch(sf::IShow& file, bool required = true) noexcept
        : detail::PatchSections<NType, NPatch>(file, required)
        , IPatch(types, recs)
    {}
};

} // namespace show
} // namespace smcp
