// nexCompId.hpp — привязка comp_id MCU ↔ id объектов Nextion (cID).
//
// Режимы (`CidMode`):
//   Compiled — id в конструкторе компонента;
//   Discover — опрос `get .id` по реестру страниц, таблица в RAM пользователя;
//   Flash    — id из буфера после `loadFromBuffer` (flash/EEPROM — код пользователя).
//
// Таблица: буфер и ёмкость задаёт приложение (`CidTable` / `CidTableStorage<N>`),
// передаётся в конструктор `Application`. Сериализация — `cidTableEncode` / `cidTableDecode` (magic NXCI).
//
// Опрос Discover: страницы в `_pages[]` с id `0..N-1` подряд без пропусков.
// `Application::setMode(Discover)` до первого `update()`; шаг машины — `Cid::updateDiscovery`
// в общем цикле UART. Ответы опроса — маршрут 0xFE/0xFE в `dispatchResponse`; `evPage` и таймаут
// страницы — `onPageChange` / `pollTick`. Пока опрос активен (`isDiscoveryBusy`), в `dispatchEvent`
// обрабатываются только смена страницы и глобальный Status; touch/MsgBox — отбрасываются.
// По завершении — `onCidPollComplete(success)`; при success режим переходит в Flash.

#pragma once

#include <cstddef>
#include <cstdint>

#include "../comp/nexComponentBase.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

class Application;

/** Режим привязки `comp_id` MCU ↔ панель Nextion. */
enum class CidMode : uint8_t {
    /** `id` задаётся в конструкторе компонента (compile-time / прошивка). */
    Compiled = 0,
    /** Опрос `.id` по всем страницам/компонентам; результат — в `Cid::table()` (сохранение flash — у пользователя). */
    Discover = 1,
    /** `id` из таблицы после загрузки буфера пользователем (`Cid::loadFromBuffer`). */
    Flash = 2,
};

/** Маршрут транзакции опроса `.id` (вне обычной таблицы page/comp). */
static constexpr uint8_t kCidRoutePageId = 0xFEu;
static constexpr uint8_t kCidRouteCompId = 0xFEu;

/** Тег транзакции опроса: индекс слота `_registry` на текущей странице (`0`…`254`). */
static constexpr uint8_t kCidPollTagBase = 0u;

/** Имя атрибута Nextion для numeric get (`get <obj>.id`). */
inline constexpr Literal kCidAttrLexeme{"id"};

/** Одна запись: страница + имя объекта HMI (`objname`) + опрошенный `id` на панели. */
struct CidRecord {
    uint8_t page_id = 0xFFu;
    uint8_t panel_id = 0xFFu;
    uint8_t name_len = 0u;
    char name[31]{};
};

/**
 * Таблица регистраций `comp_id`: буфер и ёмкость задаёт пользователь.
 *
 * @code
 *   CidRecord storage[32];
 *   CidTable table{storage, 32};
 *   MyApp app{stream, w, h, table};
 * @endcode
 *
 * Или `CidTableStorage<N>` — массив и дескриптор в одном объекте.
 */
struct CidTable {
    CidRecord* records = nullptr;
    uint16_t capacity = 0u;
    uint16_t count = 0u;

    constexpr CidTable() noexcept = default;

    constexpr CidTable(CidRecord* storage, uint16_t record_capacity) noexcept
        : records(storage)
        , capacity(record_capacity)
        , count(0u)
    {}

    /** Буфер записей привязан и ёмкость ненулевая. */
    [[nodiscard]] bool isBound() const noexcept { return records != nullptr && capacity > 0u; }

    /** Сбросить число записей; массив не обнуляется. */
    void clear() noexcept { count = 0u; }

    [[nodiscard]] const CidRecord* find(uint8_t page_id, const Literal& comp_name) const noexcept;
    [[nodiscard]] CidRecord* find(uint8_t page_id, const Literal& comp_name) noexcept;

    /** Добавить или обновить запись; `false` — переполнение, пустое имя или таблица не привязана. */
    bool upsert(uint8_t page_id, const Literal& comp_name, uint8_t panel_id) noexcept;
};

/** Массив записей + `CidTable` для передачи в `Application`. */
template<uint16_t MaxRecords>
struct CidTableStorage {
    CidRecord records[MaxRecords]{};
    CidTable table{records, MaxRecords};
};

/** Заголовок сериализованного блока таблицы (`NXCI`) для flash/EEPROM пользователя. */
struct CidBlobHeader {
    static constexpr uint32_t kMagic = 0x4E584349u; // "NXCI"
    static constexpr uint16_t kVersion = 1u;

    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t count = 0u;
};

/** Размер одной записи в wire-формате: page_id + panel_id + name_len + name[31]. */
static constexpr std::size_t kCidRecordWireSize = 34u;

/** Размер буфера под таблицу с заданной ёмкостью (число слотов записей). */
[[nodiscard]] inline constexpr std::size_t cidTableEncodedSize(uint16_t record_capacity) noexcept {
    return sizeof(CidBlobHeader) + static_cast<std::size_t>(record_capacity) * kCidRecordWireSize;
}

/** Сериализация таблицы в буфер; возвращает длину или `0` при ошибке. */
[[nodiscard]] std::size_t cidTableEncode(const CidTable& table, uint8_t* buf, std::size_t cap) noexcept;

/** Разбор блока; `false` — magic/version/длина или `count` > `table.capacity`. */
[[nodiscard]] bool cidTableDecode(CidTable& table, const uint8_t* buf, std::size_t len) noexcept;

/** Сравнение `Literal` с фрагментом C-строки фиксированной длины (имя в `CidRecord`). */
[[nodiscard]] bool cidLiteralEquals(const Literal& a, const char* b, uint8_t b_len) noexcept;

/** Внутреннее состояние опроса (не для прямого доступа снаружи). */
struct CidPoll {
    /** Фаза машины опроса всех `comp_id` в режиме `Discover`. */
    enum class Phase : uint8_t {
        Idle = 0,       /**< Опрос ещё не начат; первый `updateDiscovery` вызовет `pollBegin`. */
        SwitchPage,     /**< Ожидание `evPage` или совпадения `_currentPageId` после `switchPage`. */
        GetId,          /**< В очереди/сессии `get .id` для текущего слота реестра. */
        Done,           /**< Успех; режим переведён в `Flash`. */
        Failed,         /**< Ошибка или таймаут; `onCidPollComplete(false)`. */
    };

    Phase phase = Phase::Idle;
    uint8_t page_id = 0u;            /**< Текущая страница опроса (`0`…`_pageCount-1`, id подряд). */
    uint8_t slot = 0u;               /**< Следующий слот `Page::_registry` на текущей странице. */
    uint32_t deadline_ms = 0u;       /**< Таймаут фазы `SwitchPage` (монотонные мс из `update`). */
};

/**
 * Фасад **cID**: режимы `comp_id`, опрос `.id` — внутри `Application` (`setMode` / `getMode`).
 * Таблица регистраций передаётся в конструктор `Application` (см. `CidTable` / `CidTableStorage`).
 */
class Cid {
public:
    static constexpr uint8_t kRoutePageId = kCidRoutePageId;
    static constexpr uint8_t kRouteCompId = kCidRouteCompId;

    Cid(Application& app, CidTable& table) noexcept;

    void setMode(CidMode mode) noexcept;
    [[nodiscard]] CidMode mode() const noexcept { return _mode; }

    /** `true`, пока идёт опрос в режиме `Discover` (до `Done` / `Failed`). */
    [[nodiscard]] bool isDiscoveryBusy() const noexcept;

    [[nodiscard]] const CidTable& table() const noexcept { return _table; }
    [[nodiscard]] CidTable& table() noexcept { return _table; }
    [[nodiscard]] uint16_t tableCapacity() const noexcept { return _table.capacity; }

    /** Разбор буфера (после чтения flash) и `applyFromTable`. */
    bool loadFromBuffer(const uint8_t* buf, std::size_t len) noexcept;

    /** Применить `table()` к зарегистрированным компонентам (`Component::setId`). */
    void applyFromTable() noexcept;

    /** Текущая фаза опроса (отладка / UI прогресса). */
    [[nodiscard]] CidPoll::Phase pollPhase() const noexcept { return _poll.phase; }

    /** Шаг машины опроса в `Application::update` (режим `Discover`); UART — общий цикл приложения. */
    void updateDiscovery(uint32_t now_ms) noexcept;

    /** Ответ `get .id` для транзакции с маршрутом `kRoutePageId` / `kRouteCompId`; `tag` = индекс слота. */
    void onPollResponse(uint8_t slot, const msg::getNumeric& response) noexcept;

    /** Завершение UART-сессии опроса; при `!success` в фазе `GetId` — `pollFail`. */
    void onSessionEnd(bool success) noexcept;

    /** `evPage`: ускоряет выход из `SwitchPage`, если панель уже на целевой странице. */
    void onPageChange(uint8_t page_id) noexcept;

private:
    Application& _app;
    CidTable& _table;
    CidMode _mode = CidMode::Compiled;
    CidPoll _poll{};

    void pollBegin() noexcept;              /**< Старт Discover: список страниц, первая `switchPage`. */
    void pollFail(const char* reason) noexcept;
    void pollFinishSuccess() noexcept;      /**< `applyFromTable`, режим Flash, `onCidPollComplete(true)`. */
    void pollTick(uint32_t now_ms) noexcept;
    void pollEnqueueGetId(Component& c, uint8_t slot) noexcept;
    void pollAdvanceSlot() noexcept;          /**< Следующий компонент на странице или `pollNextPage`. */
    void pollNextPage() noexcept;
};

} // namespace nex
