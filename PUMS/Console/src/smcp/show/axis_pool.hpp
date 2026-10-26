/**
 * @file axis_pool.hpp
 * @brief smcp::show — TypeBank ("ATYP") + PatchBank ("APCH") + AxisPool (связка).
 *
 * Оба required. Patch.type_id → индекс в ATYP (kTypeNone — без типа).
 * Пропуски show::Patch.id допустимы. Profile железа в файл не входит (RAM сервера).
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "showFile.hpp"
#include "smcp/show/show_axis.hpp"

namespace smcp {
namespace show {

/** FourCC: "ATYP" / "APCH". */
inline constexpr sf::Tag kShowTypeTag = 0x50595441u;
inline constexpr sf::Tag kPatchTag = 0x48435041u;

/** Ёмкость библиотеки типов и экземпляров (пропуски show::Patch.id допустимы). */
inline constexpr uint8_t kShowTypeCount = 16u;
inline constexpr uint8_t kPatchCount = 32u;

/** Секция ATYP — библиотека ShowType. */
template <uint8_t NType = kShowTypeCount>
class TypeBank : public sf::Section<ShowType, NType> {
    static_assert(NType > 0u && NType < kTypeNone, "TypeBank: NType fits type_id");
    using Base = sf::Section<ShowType, NType>;

public:
    static constexpr uint8_t kCount = NType;

    explicit TypeBank(sf::IShow& file, bool required = true) noexcept
        : Base(file, kShowTypeTag, required)
    {}

    [[nodiscard]] bool isValid() const noexcept override
    {
        for (uint8_t i = 0; i < NType; ++i) {
            const ShowType& t = this->rec(i);
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

    [[nodiscard]] ShowType* get(uint8_t i) noexcept
    {
        return i < NType ? &this->rec(i) : nullptr;
    }
    [[nodiscard]] const ShowType* get(uint8_t i) const noexcept
    {
        return i < NType ? &this->rec(i) : nullptr;
    }
};

/**
 * Секция APCH — экземпляры show::Patch.
 * Держит ссылку на TypeBank для isValid (type_id → непустой тип).
 */
template <uint8_t NPatch = kPatchCount, uint8_t NType = kShowTypeCount>
class PatchBank : public sf::Section<Patch, NPatch> {
    static_assert(NPatch > 0u, "PatchBank: NPatch > 0");
    static_assert(NType > 0u && NType < kTypeNone, "PatchBank: NType fits type_id");
    using Base = sf::Section<Patch, NPatch>;

public:
    static constexpr uint8_t kCount = NPatch;

    PatchBank(sf::IShow& file, TypeBank<NType>& types, bool required = true) noexcept
        : Base(file, kPatchTag, required)
        , _types(types)
    {}

    [[nodiscard]] bool isValid() const noexcept override
    {
        constexpr uint8_t kKnown = static_cast<uint8_t>(PatchFlag::All);
        for (uint8_t i = 0; i < NPatch; ++i) {
            const Patch& p = this->rec(i);
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
                const ShowType* t = _types.get(p.type_id);
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

    [[nodiscard]] Patch* get(uint8_t i) noexcept
    {
        return i < NPatch ? &this->rec(i) : nullptr;
    }
    [[nodiscard]] const Patch* get(uint8_t i) const noexcept
    {
        return i < NPatch ? &this->rec(i) : nullptr;
    }

    /** Экземпляр по логическому id; нет — nullptr (пропуски допустимы). */
    [[nodiscard]] Patch* byId(uint16_t id) noexcept
    {
        if (id == kIdNone) {
            return nullptr;
        }
        for (uint8_t i = 0; i < NPatch; ++i) {
            Patch& p = this->rec(i);
            if (p.id == id) {
                return &p;
            }
        }
        return nullptr;
    }

private:
    TypeBank<NType>& _types;
};

/**
 * Связка TypeBank + PatchBank (не sf::SectionPool).
 * typeOf / isUsedType — здесь; ссылки type_id проверяет PatchBank::isValid.
 */
template <uint8_t NType = kShowTypeCount, uint8_t NPatch = kPatchCount>
class AxisPool {
public:
    static constexpr uint8_t kTypeCount = NType;
    static constexpr uint8_t kInstCount = NPatch;

    TypeBank<NType> types;
    PatchBank<NPatch, NType> patch;

    explicit AxisPool(sf::IShow& file, bool required = true) noexcept
        : types(file, required)
        , patch(file, types, required)
    {}

    [[nodiscard]] ShowType* typeOf(const Patch& p) noexcept
    {
        return p.type_id == kTypeNone ? nullptr : types.get(p.type_id);
    }
    [[nodiscard]] const ShowType* typeOf(const Patch& p) const noexcept
    {
        return p.type_id == kTypeNone ? nullptr : types.get(p.type_id);
    }

    /** Есть ли непустой экземпляр с этим type_id. */
    [[nodiscard]] bool isUsedType(uint8_t type_id) const noexcept
    {
        if (type_id == kTypeNone || type_id >= NType) {
            return false;
        }
        for (uint8_t i = 0; i < NPatch; ++i) {
            const Patch* p = patch.get(i);
            if (p != nullptr && !p->isEmpty() && p->type_id == type_id) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return types.isValid() && patch.isValid();
    }
};

} // namespace show
} // namespace smcp
