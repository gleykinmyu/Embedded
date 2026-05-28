// nexCompIdMap.hpp — карта comp_id MCU ↔ `.id` объектов Nextion (`CompIdMap`).
//
// Режимы (`CompIdMapMode`):
//   Compiled — id в конструкторе компонента;
//   Discover — опрос `get .id` по реестру страниц, таблица в RAM пользователя;
//   Flash    — id из буфера после `loadFromBuffer` (flash/EEPROM — код пользователя).
//
// Таблица: буфер и ёмкость задаёт приложение (`CompIdMapTable` / `CompIdMapTableStorage<N>`),
// передаётся в конструктор `Application`. Сериализация — `compIdMapTableEncode` / `Decode` (magic NXCI).
//
// Опрос Discover: страницы в `Application::_pageStorage` (`registerSeq`, id `0..N-1` подряд).
// `app.idMap.setMode(Discover)` до первого `update()`; шаг машины — `CompIdMap::updateDiscovery`
// в общем цикле UART. Ответы опроса — маршрут 0xFE/0xFE в `dispatchResponse`; `evPage` и таймаут
// страницы — `onPageChange` / `pollTick`. Пока опрос активен (`isDiscoveryBusy`), в `dispatchEvent`
// обрабатываются только смена страницы и глобальный Status; touch/MsgBox — отбрасываются.
// По завершении — `onCompIdMapComplete(success)`; при success режим переходит в Flash.
// Сбои Discover — процедура (`AppProcedure::CompIdMapDiscover`), не `ErrorReporter`: `pollFail` →
// `onCompIdMapComplete(false)`. Транспорт (Session timeout, NIS на маршруте 0xFE) делегируется сюда из
// `Application::checkSessionTimeout` / `dispatchResponse`, без `AppStatus::SessionTimeout` для 0xFE.

#pragma once

#include <cstddef>
#include <cstdint>

#include "../comp/nexComponentBase.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

class Application;

/** Режим привязки `comp_id` MCU ↔ панель Nextion. */
enum class CompIdMapMode : uint8_t {
    /** `id` задаётся в конструкторе компонента (compile-time / прошивка). */
    Compiled = 0,
    /** Опрос `.id` по всем страницам/компонентам; результат — в `CompIdMap::table()` (сохранение flash — у пользователя). */
    Discover = 1,
    /** `id` из таблицы после загрузки буфера пользователем (`CompIdMap::loadFromBuffer`). */
    Flash = 2,
};

/** Тег транзакции опроса: индекс слота `_registry` на текущей странице (`0`…`254`). */
static constexpr uint8_t kCompIdMapPollTagBase = 0u;

/** Имя атрибута Nextion для numeric get (`get <obj>.id`). */
inline constexpr Literal kCompIdMapAttrLexeme{"id"};

/** Одна запись: страница + имя объекта HMI (`objname`) + опрошенный `id` на панели. */
struct CompIdMapRecord {
    uint8_t page_id = 0xFFu;
    uint8_t panel_id = 0xFFu;
    uint8_t name_len = 0u;
    char name[31]{};
};

/**
 * Таблица регистраций `comp_id`: буфер и ёмкость задаёт пользователь.
 *
 * @code
 *   CompIdMapRecord storage[32];
 *   CompIdMapTable table{storage, 32};
 *   MyApp app{stream, w, h, table};
 * @endcode
 *
 * Или `CompIdMapTableStorage<N>` — массив и дескриптор в одном объекте.
 */
struct CompIdMapTable {
    CompIdMapRecord* records = nullptr;
    uint16_t capacity = 0u;
    uint16_t count = 0u;

    constexpr CompIdMapTable() noexcept = default;

    constexpr CompIdMapTable(CompIdMapRecord* storage, uint16_t record_capacity) noexcept
        : records(storage)
        , capacity(record_capacity)
        , count(0u)
    {}

    /** Буфер записей привязан и ёмкость ненулевая. */
    [[nodiscard]] bool isBound() const noexcept { return records != nullptr && capacity > 0u; }

    /** Сбросить число записей; массив не обнуляется. */
    void clear() noexcept { count = 0u; }

    [[nodiscard]] const CompIdMapRecord* find(uint8_t page_id, const Literal& comp_name) const noexcept;
    [[nodiscard]] CompIdMapRecord* find(uint8_t page_id, const Literal& comp_name) noexcept;

    /** Добавить или обновить запись; `false` — переполнение, пустое имя или таблица не привязана. */
    bool upsert(uint8_t page_id, const Literal& comp_name, uint8_t panel_id) noexcept;
};

/** Массив записей + `CompIdMapTable` для передачи в `Application`. */
template<uint16_t MaxRecords>
struct CompIdMapTableStorage {
    CompIdMapRecord records[MaxRecords]{};
    CompIdMapTable table{records, MaxRecords};
};

/** Заголовок сериализованного блока таблицы (`NXCI`) для flash/EEPROM пользователя. */
struct CompIdMapBlobHeader {
    static constexpr uint32_t kMagic = 0x4E584349u; // "NXCI"
    static constexpr uint16_t kVersion = 1u;

    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t count = 0u;
};

/** Размер одной записи в wire-формате: page_id + panel_id + name_len + name[31]. */
static constexpr std::size_t kCompIdMapRecordWireSize = 34u;

/** Размер буфера под таблицу с заданной ёмкостью (число слотов записей). */
[[nodiscard]] inline constexpr std::size_t compIdMapTableEncodedSize(uint16_t record_capacity) noexcept {
    return sizeof(CompIdMapBlobHeader) + static_cast<std::size_t>(record_capacity) * kCompIdMapRecordWireSize;
}

/** Сериализация таблицы в буфер; возвращает длину или `0` при ошибке. */
[[nodiscard]] std::size_t compIdMapTableEncode(const CompIdMapTable& table, uint8_t* buf, std::size_t cap) noexcept;

/** Разбор блока; `false` — magic/version/длина или `count` > `table.capacity`. */
[[nodiscard]] bool compIdMapTableDecode(CompIdMapTable& table, const uint8_t* buf, std::size_t len) noexcept;

/** Сравнение `Literal` с фрагментом C-строки фиксированной длины (имя в `CompIdMapRecord`). */
[[nodiscard]] bool compIdMapLiteralEquals(const Literal& a, const char* b, uint8_t b_len) noexcept;

/** Внутреннее состояние опроса (не для прямого доступа снаружи). */
struct CompIdMapPoll {
    /** Фаза машины опроса всех `comp_id` в режиме `Discover`. */
    enum class Phase : uint8_t {
        Idle = 0,       /**< Опрос ещё не начат; первый `updateDiscovery` вызовет `pollBegin`. */
        SwitchPage,     /**< Ожидание `evPage` или совпадения `_currentPageId` после `switchPage`. */
        GetId,          /**< В очереди/сессии `get .id` для текущего comp_id. */
        Done,           /**< Успех; режим переведён в `Flash`. */
        Failed,         /**< Ошибка или таймаут; `onCompIdMapComplete(false)`. */
    };

    Phase phase = Phase::Idle;
    uint8_t page_id = 0u;            /**< Текущая страница опроса (`0`…`pageCount()-1`, id подряд). */
    uint8_t scan_id = kFirstCompId;  /**< Следующий panel id при поиске очередного компонента. */
    uint8_t polled = 0u;             /**< Сколько `get .id` уже поставлено на текущей странице. */
    uint32_t deadline_ms = 0u;       /**< Таймаут фазы `SwitchPage` (монотонные мс из `update`). */
    uint32_t switch_page_last_log_ms = 0u;
    uint32_t switch_page_last_sendme_ms = 0u;
};

/**
 * Карта panel `.id` ↔ objname: режимы, опрос, persist — поле `Application::idMap`.
 * Таблица регистраций передаётся в конструктор `Application` (см. `CompIdMapTable` / `CompIdMapTableStorage`).
 */
class CompIdMap {
public:
    static constexpr uint8_t kRoutePageId = Route::kCompIdMapPollPageId;
    static constexpr uint8_t kRouteCompId = Route::kCompIdMapPollCompId;

    CompIdMap(Application& app, CompIdMapTable& table) noexcept;

    void setMode(CompIdMapMode mode) noexcept;
    [[nodiscard]] CompIdMapMode mode() const noexcept { return _mode; }

    /** `true`, пока идёт опрос в режиме `Discover` (до `Done` / `Failed`). */
    [[nodiscard]] bool isDiscoveryBusy() const noexcept;

    [[nodiscard]] const CompIdMapTable& table() const noexcept { return _table; }
    [[nodiscard]] CompIdMapTable& table() noexcept { return _table; }
    [[nodiscard]] uint16_t tableCapacity() const noexcept { return _table.capacity; }

    /** Разбор буфера (после чтения flash) и `applyFromTable`. */
    bool loadFromBuffer(const uint8_t* buf, std::size_t len) noexcept;

    /** Применить `table()` к зарегистрированным компонентам (`registry()->rebind`). */
    void applyFromTable() noexcept;

    /** Текущая фаза опроса (отладка / UI прогресса). */
    [[nodiscard]] CompIdMapPoll::Phase pollPhase() const noexcept { return _poll.phase; }

    /** Шаг машины опроса в `Application::update` (режим `Discover`); UART — общий цикл приложения. */
    void updateDiscovery(uint32_t now_ms) noexcept;

    /** Ответ `get .id` для транзакции с маршрутом `kRoutePageId` / `kRouteCompId`; `tag` = индекс слота. */
    void onPollResponse(uint8_t comp_id, const msg::getNumeric& response) noexcept;

    /** Завершение UART-сессии опроса; при `!success` в фазе `GetId` — `pollFail`. */
    void onSessionEnd(bool success) noexcept;

    /** `evPage`: ускоряет выход из `SwitchPage`, если панель уже на целевой странице. */
    void onPageChange(uint8_t page_id) noexcept;

private:
    Application& _app;
    CompIdMapTable& _table;
    CompIdMapMode _mode = CompIdMapMode::Compiled;
    CompIdMapPoll _poll{};

    void pollBegin() noexcept;              /**< Старт Discover: список страниц, первая `switchPage`. */
    void pollFail(const char* reason) noexcept;
    void pollFinishSuccess() noexcept;      /**< `applyFromTable`, режим Flash, `onCompIdMapComplete(true)`. */
    void pollTick(uint32_t now_ms) noexcept;
    void pollEnqueueGetId(Component& c, uint8_t comp_id) noexcept;
    void pollAdvanceCompId() noexcept;        /**< Следующий comp_id на странице или `pollNextPage`. */
    void pollNextPage() noexcept;
};

} // namespace nex
