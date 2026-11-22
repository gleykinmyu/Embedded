/**
 * @file comm/old/container.hpp
 * @brief comm::ProtoContainer / Proto / ProtoChild / ProtoSlots / ProtoBank.
 *
 * Один список детей. peekTx — RR, owner пробрасывается до конечного протокола.
 * onTxDone контейнер не маршрутизирует: слот зовёт owner->onTxDone(frame, ok).
 * Повторный peekTx того же протокола закрывает он сам (пока TX в полёте — false).
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "mem/list.hpp"
#include "mem/visit.hpp"
#include "comm/old/iproto.hpp"
#include "obj_bank.hpp"

namespace comm {

/** Контейнер протоколов: список детей, RR в peekTx. */
template <typename Packet>
class ProtoContainer : public IProto<Packet> {
public:
    using Proto = IProto<Packet>;

    /** Обход детей. fn → bool, true — стоп. */
    template <typename Fn>
    void forEach(Fn&& fn) noexcept
    {
        _list.forEach([&](Proto& p) -> bool { return mem::detail::visit(fn, p); });
    }

    template <typename Fn>
    void forEach(Fn&& fn) const noexcept
    {
        _list.forEach([&](const Proto& p) -> bool { return mem::detail::visit(fn, p); });
    }

    /** В хвост _list. @return false — уже в каком-то списке. */
    bool attach(Proto& p) noexcept { return _list.pushBack(p); }

    /** В голову _list (раньше остальных на onPacket / peekTx). */
    bool attachFront(Proto& p) noexcept { return _list.pushFront(p); }

    [[nodiscard]] bool empty() const noexcept
    {
        return _list.empty();
    }

    /** С _cursor, один круг. Стоп на первом, у кого есть кадр. */
    [[nodiscard]] bool peekTx(Proto*& owner, Packet& out,
                              MISC::time_t now) noexcept override
    {
        if (_list.empty()) {
            return false;
        }

        Proto* const start = (_cursor != nullptr) ? _cursor : _list.front();
        for (Proto* p = start;;) {
            Proto* got = nullptr;
            if (p->peekTx(got, out, now) && got != nullptr) {
                _cursor = _list.next(*p);
                owner = got;
                return true;
            }
            Proto* const nxt = _list.next(*p);
            p = (nxt != nullptr) ? nxt : _list.front();
            if (p == start) {
                return false;
            }
        }
    }

    [[nodiscard]] bool onPacket(const Packet& frame, MISC::time_t now) noexcept override
    {
        bool taken = false;
        forEach([&](Proto& c) -> bool {
            if (c.onPacket(frame, now)) {
                taken = true;
                return true;
            }
            return false;
        });
        return taken;
    }

    void tick(MISC::time_t now) noexcept override
    {
        forEach([&](Proto& c) { c.tick(now); });
    }

    void abort() noexcept override
    {
        _cursor = nullptr;
        _list.forEach([](Proto& c) { c.abort(); });
    }

    [[nodiscard]] bool ok() const noexcept override
    {
        bool all = true;
        forEach([&](const Proto& c) -> bool {
            if (!c.ok()) {
                all = false;
                return true;
            }
            return false;
        });
        return all;
    }

protected:
    ProtoContainer() = default;
    ~ProtoContainer() = default;

private:
    mem::List<Proto> _list;
    Proto* _cursor = nullptr; ///< следующий в _list; nullptr — с головы
};

/**
 * Вложенный контейнер. Конструктор сам делает attach и хранит родителя.
 * parent() наследник может подменить.
 */
template <typename Packet>
class ProtoChild : public ProtoContainer<Packet> {
public:
    using Parent = ProtoContainer<Packet>;

    [[nodiscard]] virtual Parent& parent() noexcept { return _parent; }
    [[nodiscard]] virtual const Parent& parent() const noexcept { return _parent; }

protected:
    explicit ProtoChild(Parent& parent) noexcept
        : _parent(parent)
    {
        parent.attach(*this);
    }

private:
    Parent& _parent;
};

/** Листовой протокол: ProtoBase + attach в _list контейнера. */
template <typename Packet>
class Proto : public ProtoBase<Packet> {
protected:
    explicit Proto(ProtoContainer<Packet>& box) noexcept
    {
        box.attach(*this);
    }
};

/**
 * Банк без массива: знает T, размера нет.
 * forEach приводит каждого ребёнка списка к T. В списке только слоты T.
 */
template <typename T, typename Packet = typename T::packet_type>
class ProtoSlots : public ProtoChild<Packet> {
    static_assert(std::is_base_of_v<IProto<Packet>, T>, "ProtoSlots: T must derive from comm::IProto");

public:
    using Slot = T;

    template <typename Fn>
    void forEach(Fn&& fn) noexcept
    {
        ProtoContainer<Packet>::forEach([&](IProto<Packet>& p) -> bool {
            return mem::detail::visit(fn, static_cast<T&>(p));
        });
    }

    template <typename Fn>
    void forEach(Fn&& fn) const noexcept
    {
        ProtoContainer<Packet>::forEach([&](const IProto<Packet>& p) -> bool {
            return mem::detail::visit(fn, static_cast<const T&>(p));
        });
    }

protected:
    explicit ProtoSlots(ProtoContainer<Packet>& parent) noexcept
        : ProtoChild<Packet>(parent)
    {}
};

/**
 * T[N] через MISC::ObjBank, наследует Base.
 * Base — наследник ProtoSlots&lt;T&gt;: тип слота уже известен, размера у базы нет.
 * По умолчанию Base = ProtoSlots&lt;T&gt;. Индекс слота в конструктор T не передаётся.
 *
 *   ProtoBank&lt;Leaf, 3&gt; leaves(box);
 *   ProtoBank&lt;Sess, N, SessionSlots&lt;Packet, Sess&gt;&gt; sessions(node);
 */
template <typename T, uint8_t N, typename Base = ProtoSlots<T, typename T::packet_type>>
class ProtoBank : public Base {
    static_assert(N >= 1u, "ProtoBank: N >= 1");
    static_assert(std::is_base_of_v<ProtoSlots<T, typename Base::packet_type>, Base>,
                  "ProtoBank: Base must derive from ProtoSlots<T>");
    static_assert(std::is_constructible_v<T, ProtoContainer<typename Base::packet_type>&>,
                  "ProtoBank: T(ProtoContainer&) must attach itself");

public:
    using SlotBase = Base;

    static constexpr uint8_t size = N;

    template <typename Arg, typename... Args>
    explicit ProtoBank(Arg& arg, Args&... args) noexcept
        : Base(arg, args...)
        , _items{*this}
    {}

    [[nodiscard]] T& at(uint8_t i) noexcept { return _items[i]; }
    [[nodiscard]] const T& at(uint8_t i) const noexcept { return _items[i]; }

private:
    MISC::ObjBank<T, N, false> _items;
};

} // namespace comm
