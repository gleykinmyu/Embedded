#pragma once

/**
 * @file nexFifo.hpp
 *
 * Две lock-free по задумке **кольцевые очереди** фиксированной ёмкости без `malloc`:
 * исходящие полиморфные `Command` (type erasure + placement new) и входящие `Message`.
 */

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "nexCommands.hpp"
#include "nexMessages.hpp"

namespace nex {

/**
 * FIFO исходящих `Command`: объекты конкретных типов создаются через **placement new** в выровненном слоте.
 *
 * Полиморфизм не храним через указатель на кучу: в слоте лежит сам `T`, а `destroy` / `toCommand` — захваченные
 * лямбды с известным на этапе `tryPush` типом `T` (деструктор и приведение `const T*` → `const Command*`).
 *
 * **Срок жизни указателя:** `peek()` возвращает `const Command*` на живой объект в буфере; он остаётся валидным
 * до `pop()` или до следующего `tryPush`, который перезапишет тот же индекс (при полной очереди это «хвост»).
 *
 * **TransparentCommand:** после отправки преамбулы объект команды ещё нужен (`transparentPayloadBytes` и логика
 * сессии). Не вызывайте `pop()`, пока вторая фаза не завершена — иначе вызовется `~T()` слишком рано.
 *
 * @tparam Capacity     число слотов в кольце
 * @tparam MaxObjectSize верхняя граница `sizeof(T)` для всех типов в `tryPush`
 * @tparam MaxAlign      выравнивание слота; для любого `T` должно выполняться `alignof(T) <= MaxAlign`
 */
template <std::size_t Capacity, std::size_t MaxObjectSize = 128,
          std::size_t MaxAlign = alignof(std::max_align_t)>
class CommandFifo {
    static_assert(Capacity > 0u);
    static_assert(MaxObjectSize >= sizeof(Command));
    static_assert(MaxAlign > 0u);

    /** Один элемент кольца: сырой буфер под `T` и две «виртуальные» операции без таблицы vtable в слоте. */
    struct Slot {
        /** Память под объект команды; после `new (storage) T(...)` здесь живёт экземпляр `T`. */
        alignas(MaxAlign) unsigned char storage[MaxObjectSize]{};
        /** Явный вызов `~T()` — у полиморфного `Command` деструктор виртуальный, но объект имеет статический тип `T`. */
        void (*destroy)(void*) noexcept = nullptr;
        /** Безопасное приведение адреса `storage` к базе `Command*` (для МИ на MCU обычно не используют). */
        const Command* (*toCommand)(const void*) noexcept = nullptr;
    };

    Slot slots_[Capacity]{};
    std::size_t head_ = 0; ///< Индекс головы: откуда читают `peek` / `pop`.
    std::size_t tail_ = 0; ///< Индекс хвоста: куда пишет следующий `tryPush`.
    std::size_t count_ = 0; ///< Число занятых слотов; инвариант `0 <= count_ <= Capacity`.

    /** Следующий индекс в кольце по модулю `Capacity`. */
    static std::size_t next(std::size_t i) noexcept { return (i + 1u) % Capacity; }

public:
    CommandFifo() noexcept = default;
    /** Разрушает все ещё лежащие в очереди объекты (`clear` → `pop` → `destroy`). */
    ~CommandFifo() noexcept { clear(); }

    CommandFifo(const CommandFifo&) = delete;
    CommandFifo& operator=(const CommandFifo&) = delete;
    CommandFifo(CommandFifo&&) = delete;
    CommandFifo& operator=(CommandFifo&&) = delete;

    bool empty() const noexcept { return count_ == 0u; }
    bool full() const noexcept { return count_ == Capacity; }
    std::size_t size() const noexcept { return count_; }

    /**
     * Копирует `cmd` в свободный слот по индексу `tail_` (placement new), записывает деструктор и `toCommand`,
     * продвигает хвост. При переполнении очереди — `false`, буфер не трогаем.
     */
    template <typename T>
    bool tryPush(const T& cmd) {
        static_assert(std::is_base_of<Command, T>::value, "T must derive from nex::Command");
        static_assert(sizeof(T) <= MaxObjectSize, "command type too large for CommandFifo slot");
        static_assert(alignof(T) <= MaxAlign, "alignment too large for CommandFifo slot");
        if (full())
            return false;
        Slot& s = slots_[tail_];
        new (s.storage) T(cmd);
        s.destroy = [](void* p) noexcept { static_cast<T*>(p)->~T(); };
        s.toCommand = [](const void* p) noexcept -> const Command* { return static_cast<const T*>(p); };
        tail_ = next(tail_);
        ++count_;
        return true;
    }

    /**
     * Указатель на команду в голове очереди без изъятия.
     * Невалиден после `pop()` и при любой модификации слота `head_` снаружи не предусмотрена — только через API.
     */
    const Command* peek() noexcept {
        if (empty())
            return nullptr;
        Slot& s = slots_[head_];
        return s.toCommand(static_cast<const void*>(s.storage));
    }

    /** Явный деструктор объекта в голове и сдвиг `head_`; для пустой очереди — no-op. */
    void pop() noexcept {
        if (empty())
            return;
        Slot& s = slots_[head_];
        s.destroy(s.storage);
        s.destroy = nullptr;
        s.toCommand = nullptr;
        head_ = next(head_);
        --count_;
    }

    /** Опустошает очередь с корректным разрушением каждого размещённого объекта. */
    void clear() noexcept {
        while (!empty())
            pop();
    }
};

/**
 * FIFO входящих `Message` (`std::variant`).
 *
 * Каждый элемент — полноценный объект `Message` в слоте: конструктор через placement new, уничтожение через
 * `std::destroy_at` после `std::launder` (правила aliasing для объекта, созданного в `unsigned char[]`).
 *
 * Пустые слоты не содержат живого `Message`; жизненный цикл задаётся только `count_` и индексами `head_`/`tail_`.
 */
template <std::size_t Capacity>
class MessageFifo {
    static_assert(Capacity > 0u);

    struct Slot {
        /** Буфер ровно под один `Message`; после записи в очередь здесь лежит вариант с ответом/событием. */
        alignas(Message) unsigned char storage[sizeof(Message)]{};
    };

    Slot slots_[Capacity]{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;

    static std::size_t next(std::size_t i) noexcept { return (i + 1u) % Capacity; }

    /**
     * Указатель на `Message`, уже построенный placement new в `storage`.
     * `launder` нужен стандарту: связать указатель с объектом, созданным в буфере символов.
     */
    static Message* slotMsg(Slot& s) noexcept { return std::launder(reinterpret_cast<Message*>(s.storage)); }
    static const Message* slotMsg(const Slot& s) noexcept {
        return std::launder(reinterpret_cast<const Message*>(s.storage));
    }

public:
    MessageFifo() noexcept = default;
    ~MessageFifo() noexcept { clear(); }

    MessageFifo(const MessageFifo&) = delete;
    MessageFifo& operator=(const MessageFifo&) = delete;
    MessageFifo(MessageFifo&&) = delete;
    MessageFifo& operator=(MessageFifo&&) = delete;

    bool empty() const noexcept { return count_ == 0u; }
    bool full() const noexcept { return count_ == Capacity; }
    std::size_t size() const noexcept { return count_; }

    /** Копирование сообщения в хвост; при полной очереди — `false`. */
    bool tryPush(const Message& m) {
        if (full())
            return false;
        Slot& s = slots_[tail_];
        new (s.storage) Message(m);
        tail_ = next(tail_);
        ++count_;
        return true;
    }

    /** Перемещение в хвост (исходный `m` остаётся в согласованном, но перенесённом состоянии). */
    bool tryPush(Message&& m) {
        if (full())
            return false;
        Slot& s = slots_[tail_];
        new (s.storage) Message(std::move(m));
        tail_ = next(tail_);
        ++count_;
        return true;
    }

    /** Неконстантный доступ к голове (например, для `std::visit` без копии). */
    Message* peek() noexcept {
        if (empty())
            return nullptr;
        return slotMsg(slots_[head_]);
    }

    const Message* peek() const noexcept {
        if (empty())
            return nullptr;
        return slotMsg(slots_[head_]);
    }

    /** Снимает голову: деструктор `Message` в слоте, индекс головы вперёд. */
    void pop() noexcept {
        if (empty())
            return;
        Slot& s = slots_[head_];
        std::destroy_at(slotMsg(s));
        head_ = next(head_);
        --count_;
    }

    /**
     * Перемещает голову в `out`, уничтожает объект в слоте; удобно, когда нужен владеющий экземпляр вне очереди.
     * @return `false`, если очередь пуста.
     */
    bool tryPop(Message& out) {
        if (empty())
            return false;
        Slot& s = slots_[head_];
        Message* p = slotMsg(s);
        out = std::move(*p);
        std::destroy_at(p);
        head_ = next(head_);
        --count_;
        return true;
    }

    void clear() noexcept {
        while (!empty())
            pop();
    }
};

} // namespace nex
