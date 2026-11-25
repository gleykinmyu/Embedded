/**
 * @file mem/list.hpp
 * @brief mem::ListNode / List — навязчивый двусвязный список (sentinel).
 *
 * Память элементов — у владельца; список держит только sentinel.
 * Элемент одновременно — не более чем в одном списке (linked()).
 * Узел = два указателя; вне списка оба nullptr.
 * ~ListNode отцепляет себя (порядок уничтожения list/элемент не важен).
 *
 *   struct Job : mem::ListNode { ... };
 *   mem::List<Job> jobs;
 *   jobs.pushBack(job);
 *   jobs.remove(job);                 // отмена из середины
 */

#pragma once

#include <type_traits>

#include "mem/visit.hpp"

namespace mem {

template <typename T>
class List;

/** База элемента списка. Копирование запрещено (адрес — идентичность). */
class ListNode {
public:
    ListNode() = default;
    /** Снять себя из списка, если ещё в нём (безопасный порядок уничтожения). */
    ~ListNode() { unlink(); }

    ListNode(const ListNode&) = delete;
    ListNode& operator=(const ListNode&) = delete;

    /** Элемент сейчас в каком-то списке. */
    [[nodiscard]] bool linked() const noexcept { return _next != nullptr; }

private:
    template <typename T>
    friend class List;

    void unlink() noexcept
    {
        if (_next == nullptr) {
            return;
        }
        _prev->_next = _next;
        _next->_prev = _prev;
        _next = nullptr;
        _prev = nullptr;
    }

    ListNode* _next = nullptr;
    ListNode* _prev = nullptr;
};

template <typename T>
class List {
    static_assert(std::is_base_of_v<ListNode, T>, "List: T must derive from ListNode");

public:
    using value_type = T;

    List() noexcept { resetSentinel(); }
    ~List() { clear(); }

    List(const List&) = delete;
    List& operator=(const List&) = delete;

    [[nodiscard]] bool empty() const noexcept { return _sen._next == &_sen; }

    [[nodiscard]] T* front() noexcept
    {
        return empty() ? nullptr : cast(_sen._next);
    }
    [[nodiscard]] const T* front() const noexcept
    {
        return empty() ? nullptr : cast(_sen._next);
    }
    [[nodiscard]] T* back() noexcept
    {
        return empty() ? nullptr : cast(_sen._prev);
    }
    [[nodiscard]] const T* back() const noexcept
    {
        return empty() ? nullptr : cast(_sen._prev);
    }

    /** Следующий за @a item. nullptr — @a item последний. @a item из этого списка. */
    [[nodiscard]] T* next(T& item) noexcept
    {
        ListNode* const n = static_cast<ListNode&>(item)._next;
        if (n == nullptr || n == &_sen) {
            return nullptr;
        }
        return cast(n);
    }

    /** O(1). @return false — элемент уже в каком-то списке. */
    [[nodiscard]] bool pushBack(T& item) noexcept
    {
        return insertBefore(_sen, item);
    }

    /** O(1). @return false — элемент уже в каком-то списке. */
    [[nodiscard]] bool pushFront(T& item) noexcept
    {
        return insertBefore(*_sen._next, item);
    }

    /** O(1). Вставить @a item перед @a pos. @a pos из этого списка.
     *  @return false — @a item уже в каком-то списке. */
    [[nodiscard]] bool insertBefore(T& pos, T& item) noexcept
    {
        return insertBefore(static_cast<ListNode&>(pos), item);
    }

    /** O(1). @return снятый элемент или nullptr. */
    T* popFront() noexcept
    {
        if (empty()) {
            return nullptr;
        }
        ListNode* const n = _sen._next;
        n->unlink();
        return cast(n);
    }

    /** O(1). @return false — элемента нет в этом списке. */
    bool remove(T& item) noexcept
    {
        ListNode& n = item;
        if (!n.linked() || !containsNode(n)) {
            return false;
        }
        n.unlink();
        return true;
    }

    /** O(n). */
    [[nodiscard]] bool contains(const T& item) const noexcept
    {
        return containsNode(static_cast<const ListNode&>(item));
    }

    /** Снять все элементы (их память не трогается). */
    void clear() noexcept
    {
        while (popFront() != nullptr) {
        }
    }

    /**
     * Обход: fn(T&). Если fn → bool, true — остановить.
     * Текущий элемент можно снять (remove / popFront) внутри fn.
     * Снимать / перекладывать другие элементы нельзя: обход оборвётся или уйдёт в чужой список.
     */
    template <typename Fn>
    void forEach(Fn&& fn)
    {
        each(*this, fn);
    }

    template <typename Fn>
    void forEach(Fn&& fn) const
    {
        each(*this, fn);
    }

private:
    void resetSentinel() noexcept
    {
        _sen._next = &_sen;
        _sen._prev = &_sen;
    }

    [[nodiscard]] bool insertBefore(ListNode& pos, T& item) noexcept
    {
        ListNode& n = item;
        if (n.linked()) {
            return false;
        }
        n._next = &pos;
        n._prev = pos._prev;
        pos._prev->_next = &n;
        pos._prev = &n;
        return true;
    }

    [[nodiscard]] bool containsNode(const ListNode& n) const noexcept
    {
        if (!n.linked()) {
            return false;
        }
        for (const ListNode* p = _sen._next; p != &_sen; p = p->_next) {
            if (p == &n) {
                return true;
            }
        }
        return false;
    }

    template <typename Self, typename Fn>
    static void each(Self& self, Fn& fn)
    {
        using E = std::conditional_t<std::is_const_v<Self>, const T, T>;
        ListNode* p = self._sen._next;
        while (p != &self._sen) {
            ListNode* const next = p->_next;
            E& e = *cast(p);
            if (detail::visit(fn, e)) {
                return;
            }
            p = next;
        }
    }

    [[nodiscard]] static T* cast(ListNode* n) noexcept { return static_cast<T*>(n); }
    [[nodiscard]] static const T* cast(const ListNode* n) noexcept
    {
        return static_cast<const T*>(n);
    }

    ListNode _sen;
};

} // namespace mem
