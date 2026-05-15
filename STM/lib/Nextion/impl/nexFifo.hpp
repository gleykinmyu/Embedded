#pragma once

/**
 * @file nexFifo.hpp
 *
 * **Исходящая очередь команд** к панели Nextion: кольцевой буфер фиксированной ёмкости без динамической памяти
 * (`malloc` / `new` на куче не используются). Входящие `Message` здесь **не** буферизуются: их разбор и доставка
 * выполняются в `Gateway::receive` → `Application::update` напрямую в колбэки.
 *
 * **Потокобезопасность:** класс не синхронизирован; предполагается вызов с одного контекста (например, главный цикл
 * + прерывания UART, если вы сами не вызываете API из ISR).
 */

#include <cstddef>
#include <new>
#include <type_traits>

#include "../core/nexCommands.hpp"

namespace nex {

/**
 * Произвольные метаданные, привязанные к элементу очереди (страница/компонент/токен для сопоставления с ответом
 * или с `onAppEvent` после завершения сессии команды).
 */
struct CommandMeta {
    uint8_t page_id = 0u;
    uint8_t component_id = 0u;
    uint16_t token = 0u;
};

/**
 * FIFO исходящих `Command`: объекты конкретных типов `T : Command` кладутся в слот через **placement new**
 * в выровненный буфер фиксированного размера `MaxObjectSize`.
 *
 * **Полиморфизм без указателя на кучу:** в слоте лежит сам `T`; в слоте же хранятся две функции-указателя,
 * захваченные лямбдами при `tryPush<T>`: `destroy` вызывает `~T()`, `toCommand` возвращает `const Command*`
 * для сериализации в `Gateway::pushCommand`.
 *
 * **Срок жизни `peek()`:** указатель валиден, пока голова очереди не снята `pop()` и пока следующий `tryPush`
 * не перезапишет тот же индекс (при полной очереди перезаписывается «хвост» после `pop` с другой стороны —
 * при нормальном FIFO это не тот же слот, что голова).
 *
 * **Transparent (NIS §1.16):** пока сессия не завершена (преамбула + сырые байты + ожидание `0xFD` и т.д.),
 * объект команды в голове ещё нужен (`transparentPayloadBytes()`). Не вызывайте `pop()`, пока верхний уровень
 * (`Application`) не завершил сессию — иначе деструктор команды разрушит данные, на которые ещё ссылается логика.
 *
 * @tparam Capacity        число слотов в кольце (число команд, которые можно поставить без изъятия головы).
 * @tparam MaxObjectSize   верхняя граница `sizeof(T)` для всех `T`, передаваемых в `tryPush`.
 * @tparam MaxAlign        выравнивание слота; для любого `T` должно быть `alignof(T) <= MaxAlign`.
 */
template <std::size_t Capacity, std::size_t MaxObjectSize = 128,
    std::size_t MaxAlign = alignof(std::max_align_t)>
class CommandFifo {
    static_assert(Capacity > 0u);
    static_assert(MaxObjectSize >= sizeof(Command));
    static_assert(MaxAlign > 0u);

    /** Один слот кольца: сырой буфер под `T` и «виртуальные» операции без vtable внутри слота. */
    struct Slot {
        /** Память под объект команды; после `new (storage) T(cmd)` здесь живёт `T`. */
        alignas(MaxAlign) unsigned char storage[MaxObjectSize]{};
        /** Явный вызов `~T()` (у `Command` деструктор виртуальный, но объект имеет статический тип `T`). */
        void (*destroy)(void*) noexcept = nullptr;
        /** Приведение `storage` к `const Command*` для `serialize` / флагов. */
        const Command* (*toCommand)(const void*) noexcept = nullptr;
        /** Метаданные, переданные в `tryPush(..., meta)`. */
        CommandMeta meta{};
    };

    Slot slots_[Capacity]{};
    std::size_t head_ = 0; ///< Индекс головы: откуда `peek` / `pop`.
    std::size_t tail_ = 0; ///< Индекс хвоста: куда пишет следующий `tryPush`.
    std::size_t count_ = 0; ///< Число занятых слотов, `0 … Capacity`.

    static std::size_t next(std::size_t i) noexcept { return (i + 1u) % Capacity; }

public:
    CommandFifo() noexcept = default;
    /** Уничтожает все объекты в очереди (`clear` → `pop` → `destroy`). */
    ~CommandFifo() noexcept { clear(); }

    CommandFifo(const CommandFifo&) = delete;
    CommandFifo& operator=(const CommandFifo&) = delete;
    CommandFifo(CommandFifo&&) = delete;
    CommandFifo& operator=(CommandFifo&&) = delete;

    bool empty() const noexcept { return count_ == 0u; }
    bool full() const noexcept { return count_ == Capacity; }
    std::size_t size() const noexcept { return count_; }

    /**
     * Копирует `cmd` в слот `tail_` (placement new), записывает `destroy` / `toCommand`, продвигает хвост.
     * @return `false`, если очередь полна (объект не создан).
     */
    template <typename T>
    bool tryPush(const T& cmd, CommandMeta meta = {}) {
        static_assert(std::is_base_of<Command, T>::value, "T must derive from nex::Command");
        static_assert(sizeof(T) <= MaxObjectSize, "command type too large for CommandFifo slot");
        static_assert(alignof(T) <= MaxAlign, "alignment too large for CommandFifo slot");
        if (full())
            return false;
        Slot& s = slots_[tail_];
        new (s.storage) T(cmd);
        s.destroy = [](void* p) noexcept { static_cast<T*>(p)->~T(); };
        s.toCommand = [](const void* p) noexcept -> const Command* { return static_cast<const T*>(p); };
        s.meta = meta;
        tail_ = next(tail_);
        ++count_;
        return true;
    }

    /** Указатель на команду в голове без изъятия; `nullptr`, если очередь пуста. */
    const Command* peek() noexcept {
        if (empty())
            return nullptr;
        Slot& s = slots_[head_];
        return s.toCommand(static_cast<const void*>(s.storage));
    }

    /** Метаданные головы или `nullptr`, если очередь пуста. */
    const CommandMeta* peekMeta() const noexcept {
        if (empty())
            return nullptr;
        return &slots_[head_].meta;
    }

    /** Деструктор объекта в голове, сдвиг `head_`; для пустой очереди — no-op. */
    void pop() noexcept {
        if (empty())
            return;
        Slot& s = slots_[head_];
        s.destroy(s.storage);
        s.destroy = nullptr;
        s.toCommand = nullptr;
        s.meta = {};
        head_ = next(head_);
        --count_;
    }

    /** Опустошает очередь с корректным разрушением каждого объекта. */
    void clear() noexcept {
        while (!empty())
            pop();
    }
};

} // namespace nex
