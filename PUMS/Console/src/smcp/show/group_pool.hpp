/**
 * @file group_pool.hpp
 * @brief test: CGroup<MaxMechs> : PoolView<GHDR, GSEL>. Не пульт.
 *
 * Local selection — биты 0…MaxMechs-1; слов GSEL = (MaxMechs+31)/32.
 * Recall пакует segment::Selection по segment::console::Axis: local_id → server_id + axis_id.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "section_pool.hpp"
#include "segment/console.hpp"
#include "smcp/show/group.hpp"

namespace smcp {
namespace test {

inline constexpr sf::Tag kGroupHeaderTag = 0x52444847u; /**< "GHDR". */
inline constexpr sf::Tag kGroupSlotTag = 0x4C455347u;   /**< "GSEL". */

/** Заголовок группы в пуле. PoolRef вместо segment::Selection. */
struct alignas(uint32_t) GroupHeader {
    uint8_t id = 0;
    REG::BitMask<show::Group::Flag> flag;
    uint8_t reserved_1[2]{};
    sf::PoolRef pool{};
    char name[show::kGroupNameSize]{};
    uint8_t reserved_2[8]{};

    [[nodiscard]] constexpr bool isBlocked() const noexcept
    {
        return flag.any(show::Group::Flag::Blocked);
    }

    void setName(const char* group_name) noexcept
    {
        if (group_name == nullptr) {
            name[0] = '\0';
            return;
        }
        std::strncpy(name, group_name, show::kGroupNameSize - 1u);
        name[show::kGroupNameSize - 1u] = '\0';
    }

    void setBlocked(bool on = true) noexcept
    {
        if (on) {
            flag.set(show::Group::Flag::Blocked);
        } else {
            flag.clear(show::Group::Flag::Blocked);
        }
    }

    /** id и pool не трогает. */
    void clear() noexcept
    {
        flag = {};
        std::memset(reserved_1, 0, sizeof(reserved_1));
        std::memset(name, 0, sizeof(name));
        std::memset(reserved_2, 0, sizeof(reserved_2));
    }
};

static_assert(sizeof(GroupHeader) == show::kGroupWireSize);
static_assert(alignof(GroupHeader) == alignof(uint32_t));
static_assert(offsetof(GroupHeader, pool) == 4);
static_assert(offsetof(GroupHeader, name) == 8);

template <uint8_t MaxMechs>
[[nodiscard]] constexpr uint16_t selWordCount() noexcept
{
    return static_cast<uint16_t>((static_cast<uint16_t>(MaxMechs) + 31u) / 32u);
}

template <uint8_t MaxMechs>
class CGroup;
template <uint8_t MaxMechs>
class IGroupBank;

template <uint8_t MaxMechs>
class IGroupBank {
public:
    using View = sf::PoolView<GroupHeader, segment::Selection>;

    virtual ~IGroupBank() = default;

    [[nodiscard]] virtual View viewAt(uint16_t i) noexcept = 0;
    [[nodiscard]] virtual sf::IPool<GroupHeader, segment::Selection>& pool() noexcept = 0;
    [[nodiscard]] virtual segment::Console& console() noexcept = 0;
    virtual void markEdited() noexcept = 0;
};

/**
 * Вид на GHDR + GSEL. Тип банка (NH/NS) не входит.
 * Копия жива после compact чужой группы: at/size/header перечитывают first.
 */
template <uint8_t MaxMechs>
class CGroup : public sf::PoolView<GroupHeader, segment::Selection> {
    static_assert(MaxMechs > 0u, "CGroup: MaxMechs > 0");

    using Base = sf::PoolView<GroupHeader, segment::Selection>;

public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint16_t kSelWords = selWordCount<MaxMechs>();

    using Base::at;
    using Base::header;
    using Base::isEmpty;
    using Base::size;

    enum class Result : uint8_t {
        Ok = 0,
        Empty,
        Occupied,
        Blocked,
        OverlapsBlocked,
        NotSent,
    };

    CGroup() noexcept = default;

    CGroup(const Base& view, IGroupBank<MaxMechs>& bank) noexcept
        : Base(view)
        , _bank(&bank)
    {}

    [[nodiscard]] uint8_t id() const noexcept
    {
        const GroupHeader* const h = header();
        return h != nullptr ? h->id : 0u;
    }
    [[nodiscard]] REG::BitMask<show::Group::Flag> flags() const noexcept
    {
        const GroupHeader* const h = header();
        return h != nullptr ? h->flag : REG::BitMask<show::Group::Flag>{};
    }
    [[nodiscard]] const char* name() const noexcept
    {
        const GroupHeader* const h = header();
        return h != nullptr ? h->name : "";
    }
    [[nodiscard]] bool isBlocked() const noexcept
    {
        const GroupHeader* const h = header();
        return h != nullptr && h->isBlocked();
    }
    [[nodiscard]] bool contains(uint16_t local_id) const noexcept
    {
        if (local_id >= MaxMechs) {
            return false;
        }
        const segment::Selection* const s = at(wordOf(local_id));
        return s != nullptr && s->contains(bitOf(local_id));
    }

    void setBlocked(bool on = true) noexcept
    {
        GroupHeader* const h = header();
        if (h == nullptr || _bank == nullptr) {
            return;
        }
        if (h->isBlocked() == on) {
            return;
        }
        h->setBlocked(on);
        _bank->markEdited();
    }

    /** Включить/снять local_id; хвост пустых GSEL сжимается. */
    [[nodiscard]] bool set(uint16_t local_id, bool on) noexcept
    {
        if (!(*this) || local_id >= MaxMechs) {
            return false;
        }
        const uint16_t w = wordOf(local_id);
        const uint8_t bit = bitOf(local_id);
        if (on) {
            if (w >= size() && !resize(static_cast<uint16_t>(w + 1u))) {
                return false;
            }
            segment::Selection* const s = at(w);
            if (s == nullptr) {
                return false;
            }
            s->add(bit);
            return true;
        }
        segment::Selection* const s = at(w);
        if (s != nullptr) {
            s->remove(bit);
        }
        trim();
        return true;
    }

    /**
     * Select по серверам: local → Patch.route (server_id + axis_id).
     * Пока route не на segment::console::Axis — NotSent.
     */
    [[nodiscard]] Result recall() noexcept
    {
        if (isEmpty()) {
            return Result::Empty;
        }
        if (isBlocked()) {
            return Result::Blocked;
        }
        if (_bank == nullptr) {
            return Result::Empty;
        }
        if (overlapsBlocked()) {
            return Result::OverlapsBlocked;
        }
        (void)_bank->console();
        return Result::NotSent;
    }

    /**
     * Записать local-биты (слово 0 = оси 0…31). Старшие слова снимаются.
     * Occupied снимается @a confirmed; OverlapsBlocked — нет.
     */
    [[nodiscard]] Result record(segment::Selection local, const char* group_name = nullptr,
                                bool confirmed = false) noexcept
    {
        if (!(*this) || _bank == nullptr || local.empty()) {
            return Result::Empty;
        }
        if (!isEmpty() && !confirmed) {
            return Result::Occupied;
        }
        const segment::Selection words[1]{local};
        if (overlapsBlocked(words, 1u)) {
            return Result::OverlapsBlocked;
        }
        if (!resize(1u)) {
            return Result::NotSent;
        }
        segment::Selection* const dst = at(0u);
        if (dst == nullptr) {
            return Result::NotSent;
        }
        *dst = local;
        if (group_name != nullptr && group_name[0] != '\0') {
            header()->setName(group_name);
        }
        _bank->markEdited();
        return Result::Ok;
    }

    /** Снять выделенные сейчас оси пульта в local-биты. */
    [[nodiscard]] Result recordSelected(bool confirmed = false) noexcept
    {
        if (!(*this) || _bank == nullptr) {
            return Result::Empty;
        }
        if (!isEmpty() && !confirmed) {
            return Result::Occupied;
        }
        segment::Console& cons = _bank->console();
        segment::Selection words[kSelWords]{};
        uint16_t used = 0u;
        bool any = false;
        const uint16_t n = cons.axisCapacity();
        for (uint16_t i = 0u; i < n; ++i) {
            segment::console::Axis* const m = cons.axis(i);
            if (m == nullptr || !m->isSelected()) {
                continue;
            }
            const uint16_t lid = i;
            if (lid >= MaxMechs) {
                continue;
            }
            any = true;
            const uint16_t w = wordOf(lid);
            words[w].add(bitOf(lid));
            if (static_cast<uint16_t>(w + 1u) > used) {
                used = static_cast<uint16_t>(w + 1u);
            }
        }
        if (!any) {
            return Result::Empty;
        }
        if (overlapsBlocked(words, used)) {
            return Result::OverlapsBlocked;
        }
        if (!resize(used)) {
            return Result::NotSent;
        }
        for (uint16_t w = 0u; w < used; ++w) {
            *at(w) = words[w];
        }
        _bank->markEdited();
        return Result::Ok;
    }

    [[nodiscard]] bool rename(const char* group_name) noexcept
    {
        if (group_name == nullptr || group_name[0] == '\0' || isEmpty()) {
            return false;
        }
        GroupHeader* const h = header();
        if (h == nullptr || _bank == nullptr) {
            return false;
        }
        h->setName(group_name);
        _bank->markEdited();
        return true;
    }

    [[nodiscard]] bool clear(bool confirmed = false) noexcept
    {
        if (isEmpty()) {
            return true;
        }
        if (!confirmed || _bank == nullptr) {
            return false;
        }
        clearSlots();
        GroupHeader* const h = header();
        if (h != nullptr) {
            h->clear();
        }
        _bank->markEdited();
        return true;
    }

private:
    [[nodiscard]] static constexpr uint16_t wordOf(uint16_t local_id) noexcept
    {
        return static_cast<uint16_t>(local_id / smcp::kMechCount);
    }
    [[nodiscard]] static constexpr uint8_t bitOf(uint16_t local_id) noexcept
    {
        return static_cast<uint8_t>(local_id % smcp::kMechCount);
    }

    [[nodiscard]] bool overlapsBlocked() const noexcept
    {
        segment::Selection words[kSelWords]{};
        const uint16_t n = size();
        const uint16_t used = (n < kSelWords) ? n : kSelWords;
        for (uint16_t w = 0u; w < used; ++w) {
            const segment::Selection* const s = at(w);
            if (s != nullptr) {
                words[w] = *s;
            }
        }
        return overlapsBlocked(words, used);
    }

    [[nodiscard]] bool overlapsBlocked(const segment::Selection* words, uint16_t nwords) const noexcept
    {
        if (_bank == nullptr || words == nullptr || nwords == 0u) {
            return false;
        }
        const uint8_t self = id();
        const uint16_t n = _bank->pool().headerCount;
        for (uint16_t i = 0u; i < n; ++i) {
            CGroup other(_bank->viewAt(i), *_bank);
            if (!other || other.id() == self || !other.isBlocked()) {
                continue;
            }
            const uint16_t m = (nwords < other.size()) ? nwords : other.size();
            for (uint16_t w = 0u; w < m; ++w) {
                const segment::Selection* const b = other.at(w);
                if (b != nullptr && (words[w] & *b).any()) {
                    return true;
                }
            }
        }
        return false;
    }

    void trim() noexcept
    {
        uint16_t n = size();
        while (n > 0u) {
            const segment::Selection* const last = at(static_cast<uint16_t>(n - 1u));
            if (last != nullptr && last->any()) {
                break;
            }
            --n;
        }
        if (n == 0u) {
            clearSlots();
        } else if (n != size()) {
            (void)resize(n);
        }
    }

    IGroupBank<MaxMechs>* _bank = nullptr;
};

/**
 * GHDR[MaxGroups] + GSEL[MaxGroups * kSelWords].
 * Ёмкость Show — +2 секции. После load — syncFreeTop().
 */
template <uint8_t MaxMechs, uint8_t MaxGroups = show::kGroupMaxCount>
class CGroupBank : public sf::SectionPool<GroupHeader, segment::Selection, MaxGroups,
                                          static_cast<uint16_t>(MaxGroups) * selWordCount<MaxMechs>()>,
                   public IGroupBank<MaxMechs> {
    static_assert(MaxMechs > 0u, "CGroupBank: MaxMechs > 0");
    static_assert(MaxGroups > 0u, "CGroupBank: MaxGroups > 0");

    using Pool = sf::SectionPool<GroupHeader, segment::Selection, MaxGroups,
                                 static_cast<uint16_t>(MaxGroups) * selWordCount<MaxMechs>()>;

public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kCount = MaxGroups;
    static constexpr uint16_t kSelWords = selWordCount<MaxMechs>();

    CGroupBank(sf::IShow& file, segment::Console& console, bool required = true) noexcept
        : Pool(file, kGroupHeaderTag, kGroupSlotTag, required)
        , _console(console)
    {
        for (uint8_t i = 0u; i < MaxGroups; ++i) {
            this->headerAt(i)->id = i;
        }
    }

    void clearData() noexcept
    {
        Pool::clearData();
        for (uint8_t i = 0u; i < MaxGroups; ++i) {
            this->headerAt(i)->id = i;
        }
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        if (!this->isLayoutOk()) {
            return false;
        }
        constexpr uint8_t kKnown = static_cast<uint8_t>(show::Group::Flag::Blocked)
            | static_cast<uint8_t>(show::Group::Flag::Atomic);
        auto* const self = const_cast<CGroupBank*>(this);
        for (uint8_t i = 0u; i < MaxGroups; ++i) {
            const GroupHeader* const h = self->headerAt(i);
            if ((h->flag.raw() & static_cast<uint8_t>(~kKnown)) != 0u) {
                return false;
            }
            if (std::memchr(h->name, '\0', show::kGroupNameSize) == nullptr) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] CGroup<MaxMechs> operator[](uint8_t i) noexcept
    {
        return CGroup<MaxMechs>(this->at(i), *this);
    }

    [[nodiscard]] typename IGroupBank<MaxMechs>::View viewAt(uint16_t i) noexcept override
    {
        return this->at(i);
    }
    [[nodiscard]] sf::IPool<GroupHeader, segment::Selection>& pool() noexcept override { return *this; }
    [[nodiscard]] segment::Console& console() noexcept override { return _console; }
    void markEdited() noexcept override { Pool::markEdited(); }

private:
    segment::Console& _console;
};

static_assert(CGroup<24>::kSelWords == 1u);
static_assert(CGroup<32>::kSelWords == 1u);
static_assert(CGroup<33>::kSelWords == 2u);
static_assert(CGroup<64>::kSelWords == 2u);

} // namespace test
} // namespace smcp
