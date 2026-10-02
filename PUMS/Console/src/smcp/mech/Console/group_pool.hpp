/**
 * @file group_pool.hpp
 * @brief test: CGroup<MaxMechs> : PoolView<GHDR, GSEL>. Не пульт.
 *
 * Local selection — биты 0…MaxMechs-1; слов GSEL = (MaxMechs+31)/32.
 * Recall пакует Selection по CMech: local_id → server_id + mech_id.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "section_pool.hpp"
#include "smcp/mech/Console/console.hpp"
#include "smcp/mech/group.hpp"

namespace smcp {
namespace test {

inline constexpr uint32_t kGroupHeaderTag = 0x52444847u; /**< "GHDR". */
inline constexpr uint32_t kGroupSlotTag = 0x4C455347u;   /**< "GSEL". */

/** Заголовок группы в пуле. PoolRef вместо Selection. */
struct alignas(uint32_t) GroupHeader {
    uint8_t id = 0;
    REG::BitMask<Group::Flag> flag;
    uint8_t reserved_1[2]{};
    sf::PoolRef pool{};
    char name[kGroupNameSize]{};
    uint8_t reserved_2[8]{};

    [[nodiscard]] constexpr bool isBlocked() const noexcept
    {
        return flag.any(Group::Flag::Blocked);
    }

    void setName(const char* group_name) noexcept
    {
        if (group_name == nullptr) {
            name[0] = '\0';
            return;
        }
        std::strncpy(name, group_name, kGroupNameSize - 1u);
        name[kGroupNameSize - 1u] = '\0';
    }

    void setBlocked(bool on = true) noexcept
    {
        if (on) {
            flag.set(Group::Flag::Blocked);
        } else {
            flag.clear(Group::Flag::Blocked);
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

static_assert(sizeof(GroupHeader) == kGroupWireSize);
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
    using View = sf::PoolView<GroupHeader, Selection>;

    virtual ~IGroupBank() = default;

    [[nodiscard]] virtual View viewAt(uint16_t i) noexcept = 0;
    [[nodiscard]] virtual sf::IPool<GroupHeader, Selection>& pool() noexcept = 0;
    [[nodiscard]] virtual IConsole& console() noexcept = 0;
    virtual void markEdited() noexcept = 0;
};

/**
 * Вид на GHDR + GSEL. Тип банка (NH/NS) не входит.
 * Копия жива после compact чужой группы: at/size/header перечитывают first.
 */
template <uint8_t MaxMechs>
class CGroup : public sf::PoolView<GroupHeader, Selection> {
    static_assert(MaxMechs > 0u, "CGroup: MaxMechs > 0");

    using Base = sf::PoolView<GroupHeader, Selection>;

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
    [[nodiscard]] REG::BitMask<Group::Flag> flags() const noexcept
    {
        const GroupHeader* const h = header();
        return h != nullptr ? h->flag : REG::BitMask<Group::Flag>{};
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
    [[nodiscard]] bool contains(uint8_t local_id) const noexcept
    {
        if (local_id >= MaxMechs) {
            return false;
        }
        const Selection* const s = at(wordOf(local_id));
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
    [[nodiscard]] bool set(uint8_t local_id, bool on) noexcept
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
            Selection* const s = at(w);
            if (s == nullptr) {
                return false;
            }
            s->add(bit);
            return true;
        }
        Selection* const s = at(w);
        if (s != nullptr) {
            s->remove(bit);
        }
        trim();
        return true;
    }

    /**
     * Select по серверам: бит local → CMech::id() в маске этого server_id.
     * Билет queued IGroupConsole не ставит (там ещё smcp::CGroup).
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
        IConsole& cons = _bank->console();
        struct Wire {
            uint8_t sid = 0;
            Selection sel{};
        };
        Wire wires[MaxMechs]{};
        uint8_t nw = 0u;
        const uint8_t n = cons.localMechCount();
        for (uint8_t i = 0u; i < n; ++i) {
            CMech* const m = cons.localMech(i);
            if (m == nullptr || !contains(m->localId())) {
                continue;
            }
            const uint8_t sid = m->serverId();
            if (!msg::isServerId(sid)) {
                continue;
            }
            uint8_t w = 0u;
            for (; w < nw; ++w) {
                if (wires[w].sid == sid) {
                    break;
                }
            }
            if (w == nw) {
                if (nw >= MaxMechs) {
                    return Result::NotSent;
                }
                wires[nw].sid = sid;
                ++nw;
            }
            wires[w].sel.add(m->id());
        }
        if (nw == 0u) {
            return Result::Empty;
        }
        for (uint8_t w = 0u; w < nw; ++w) {
            ServerSession* const s = cons.server(wires[w].sid);
            if (s == nullptr || !s->setSelection(wires[w].sel)) {
                return Result::NotSent;
            }
        }
        return Result::Ok;
    }

    /**
     * Записать local-биты (слово 0 = оси 0…31). Старшие слова снимаются.
     * Occupied снимается @a confirmed; OverlapsBlocked — нет.
     */
    [[nodiscard]] Result record(Selection local, const char* group_name = nullptr,
                                bool confirmed = false) noexcept
    {
        if (!(*this) || _bank == nullptr || local.empty()) {
            return Result::Empty;
        }
        if (!isEmpty() && !confirmed) {
            return Result::Occupied;
        }
        const Selection words[1]{local};
        if (overlapsBlocked(words, 1u)) {
            return Result::OverlapsBlocked;
        }
        if (!resize(1u)) {
            return Result::NotSent;
        }
        Selection* const dst = at(0u);
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
        IConsole& cons = _bank->console();
        Selection words[kSelWords]{};
        uint16_t used = 0u;
        bool any = false;
        const uint8_t n = cons.localMechCount();
        for (uint8_t i = 0u; i < n; ++i) {
            CMech* const m = cons.localMech(i);
            if (m == nullptr || !m->isSelected()) {
                continue;
            }
            const uint8_t lid = m->localId();
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
    [[nodiscard]] static constexpr uint16_t wordOf(uint8_t local_id) noexcept
    {
        return static_cast<uint16_t>(local_id / smcp::kMechCount);
    }
    [[nodiscard]] static constexpr uint8_t bitOf(uint8_t local_id) noexcept
    {
        return static_cast<uint8_t>(local_id % smcp::kMechCount);
    }

    [[nodiscard]] bool overlapsBlocked() const noexcept
    {
        Selection words[kSelWords]{};
        const uint16_t n = size();
        const uint16_t used = (n < kSelWords) ? n : kSelWords;
        for (uint16_t w = 0u; w < used; ++w) {
            const Selection* const s = at(w);
            if (s != nullptr) {
                words[w] = *s;
            }
        }
        return overlapsBlocked(words, used);
    }

    [[nodiscard]] bool overlapsBlocked(const Selection* words, uint16_t nwords) const noexcept
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
                const Selection* const b = other.at(w);
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
            const Selection* const last = at(static_cast<uint16_t>(n - 1u));
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
template <uint8_t MaxMechs, uint8_t MaxGroups = kGroupMaxCount>
class CGroupBank : public sf::SectionPool<GroupHeader, Selection, MaxGroups,
                                          static_cast<uint16_t>(MaxGroups) * selWordCount<MaxMechs>()>,
                   public IGroupBank<MaxMechs> {
    static_assert(MaxMechs > 0u, "CGroupBank: MaxMechs > 0");
    static_assert(MaxGroups > 0u, "CGroupBank: MaxGroups > 0");

    using Pool = sf::SectionPool<GroupHeader, Selection, MaxGroups,
                                 static_cast<uint16_t>(MaxGroups) * selWordCount<MaxMechs>()>;

public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kCount = MaxGroups;
    static constexpr uint16_t kSelWords = selWordCount<MaxMechs>();

    CGroupBank(sf::IShow& file, IConsole& console, bool required = true) noexcept
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
        constexpr uint8_t kKnown = static_cast<uint8_t>(Group::Flag::Blocked)
            | static_cast<uint8_t>(Group::Flag::Atomic);
        auto* const self = const_cast<CGroupBank*>(this);
        for (uint8_t i = 0u; i < MaxGroups; ++i) {
            const GroupHeader* const h = self->headerAt(i);
            if ((h->flag.raw() & static_cast<uint8_t>(~kKnown)) != 0u) {
                return false;
            }
            if (std::memchr(h->name, '\0', kGroupNameSize) == nullptr) {
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
    [[nodiscard]] sf::IPool<GroupHeader, Selection>& pool() noexcept override { return *this; }
    [[nodiscard]] IConsole& console() noexcept override { return _console; }
    void markEdited() noexcept override { Pool::markEdited(); }

private:
    IConsole& _console;
};

static_assert(CGroup<24>::kSelWords == 1u);
static_assert(CGroup<32>::kSelWords == 1u);
static_assert(CGroup<33>::kSelWords == 2u);
static_assert(CGroup<64>::kSelWords == 2u);

} // namespace test
} // namespace smcp
