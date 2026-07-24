/**
 * @file mconsole.hpp
 * @brief Модель консоли: режимы, механизмы, группы, шоуфайл (SETT пульта + GRUP).
 *
 * Держит ссылку на MBrowser: openShow / saveShow / saveShowAs работают через него.
 *
 * Select → tx(); Telemetry ← rx() + poll(). Шоу/группы/isolate — на консоли.
 * Пока без CAN: loopback с MServer (main) через очереди + optional flush hook.
 *
 * TODO(архитектура): разнести слои, как в Nextion (AppUI / Session / Gateway) —
 * сейчас MConsole слишком раздут (шоуфайл + GRUP/режимы/isolate + SMCP rx/tx/poll).
 *
 * Вариант A — транспорт рядом с моделью (предпочтительно по аналогии с Nextion):
 *   - MConsole: только домен пульта (Mode, Group, SETT/GRUP, pressMech/pressGroup, …);
 *   - SmcpLink / PacketHub: очереди Packet, pkt_id, flush, encode Select/…,
 *     poll RX → dispatch по MsgId;
 *   - Mech (или Mechanics): onTelemetry / исходящие запросы осей через owner→link.
 * UI: takeTelemetryDirty / applyTelemetryUi остаётся снаружи модели.
 *
 * Вариант B — объект Mechanics { Mech[] }:
 *   + локально: Telemetry, Select/Deselect, SetTarget, ResetFault по mech_id;
 *   − не всё SMCP про оси: Heartbeat/SysInfo/Ack/Nack/CriticalErr, будущие
 *     сегментные команды без mech_id, маршрутизация dst/src — это уровень link;
 *   − show-Blocked / isolate / лимит UI — по-прежнему зона MConsole, не Mechanics.
 *   Итого: Mechanics = «оси + их сообщения»; link = остальной протокол;
 *   MConsole оркестрирует UI-домен и держит ссылку на оба.
 *
 * Не смешивать: шоуфайл (file::) ≠ SMCP-on-CAN; Blocked-группы шоу ≠ приводной Status.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "smcp/group.hpp"
#include "smcp/message.hpp"
#include "smcp/packet_queue.hpp"
#include "smcp/show_file.hpp"
#include "model/mech.hpp"

class MBrowser;

class MConsole {
public:
    /* ========== Константы ========== */
    static constexpr std::size_t kMechCount = 24u;
    static constexpr uint8_t kBlockedGroupId = 0u;
    /** Секция настроек пульта в шоуфайле (не SMCP) — FourCC "SETT". */
    static constexpr uint32_t kSettingsSectionTag = 0x54544553u;
    static constexpr std::size_t kSettingsWireSize = 8u;

    static constexpr std::size_t kRxDepth = 32u;
    static constexpr std::size_t kTxDepth = 16u;

    /** После push в tx — синхронный pump (loopback / CAN TX). */
    using FlushHook = void (*)() noexcept;

    /* ========== Типы ========== */
    enum class Mode : uint8_t {
        Work = 0,
        Block = 1,
        Show = 2,
    };

    /** Результат блокировки лебёдки/группы в Mode::Block. */
    enum class BlockResult : uint8_t {
        NoChange = 0,
        Changed,
        Rejected, /**< Ручная блокировка запрещена (есть blocked-группы). */
        Warning,  /**< Группа заблокирована, есть пересечение — см. blockMessage(). */
    };

    enum class Status : uint8_t {
        Ok = 0,
        /* Шоуфайл / носитель */
        NoShowOpen,         /**< Пустой путь / нет открытого шоу. */
        TemplateProtected,  /**< Имя файла — шаблон (нельзя сохранять). */
        BadMagic,           /**< Неверная сигнатура SMCP. */
        BadVersion,         /**< Несовместимая версия шоуфайла. */
        BadHeaderCrc,       /**< Не совпал CRC заголовка. */
        BadBodyCrc,         /**< Не совпал CRC тела. */
        BadLayout,          /**< Повреждённая структура / каталог. */
        Truncated,          /**< Файл короче заявленного размера. */
        MissingGrup,        /**< Нет секции GRUP. */
        BadGroups,          /**< Некорректные записи групп. */
        IoError,            /**< Ошибка носителя (open/read/write). */
        /* Группы */
        InvalidGroup,       /**< Неверный id / служебная Blocked. */
        NoSelection,        /**< Нет выделенных лебёдок для Record. */
        GroupOccupied,      /**< Группа не пуста (Record/Clear → Yes/No). */
        /* Браузер */
        BrowserFault,       /**< Детали — MBrowser::getStatus() / statusText. */
        InvalidName,        /**< Недопустимое имя (консоль, до браузера). */
    };

    /** Настройки пульта; пишутся в шоуфайл секцией SETT (константы — в MConsole). */
    struct Settings {
        bool isolateGroup = false; /**< true: чужие лебёдки Disabled на UI; recall по-прежнему выделяет группу. */
        uint8_t reserved[7]{};
    };
    static_assert(sizeof(Settings) == kSettingsWireSize, "MConsole::Settings size");

    /* ========== Конструктор / зеркало ========== */
    explicit MConsole(MBrowser& browser,
                      uint8_t console_id = 1u) noexcept;

    /** Опциональное зеркало (любой IFile: W25Q-сектор, …). */
    void setMirror(BIF::IFile* mirror) noexcept { _mirror = mirror; }
    /** Boot: прочитать зеркало, если setMirror. */
    [[nodiscard]] bool restoreMirror() noexcept;

    /** Loopback/CAN: вызвать после постановки команд в tx. */
    void setFlushHook(FlushHook hook) noexcept { _flush = hook; }

    /**
     * Поставить Select/Deselect в tx (вызывает Mech через owner).
     * Маска может содержать несколько осей.
     */
    [[nodiscard]] bool pushSelect(smcp::msg::Select::Action action,
                                  smcp::Selection selection) noexcept;

    [[nodiscard]] smcp::msg::PacketQueue<kRxDepth>& rx() noexcept { return _rx; }
    [[nodiscard]] smcp::msg::PacketQueue<kTxDepth>& tx() noexcept { return _tx; }
    [[nodiscard]] const smcp::msg::PacketQueue<kRxDepth>& rx() const noexcept { return _rx; }
    [[nodiscard]] const smcp::msg::PacketQueue<kTxDepth>& tx() const noexcept { return _tx; }

    /** Разобрать rx (Telemetry) → Mech::onTelemetry; копит dirty для UI. */
    void poll() noexcept;

    /**
     * Биты mech_id, по которым пришла Telemetry с прошлого take.
     * UI читает и сбрасывает.
     */
    [[nodiscard]] uint32_t takeTelemetryDirty() noexcept;

    /* ========== Статус ========== */
    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    [[nodiscard]] static const char* statusText(Status status) noexcept;

    /* ========== Режим ========== */
    [[nodiscard]] Mode mode() const noexcept { return _mode; }
    void setMode(Mode mode) noexcept;
    /**
     * Переключить режим-кнопку: повторный вызов того же режима → Work,
     * иначе → @a mode. Work как аргумент всегда ставит Work.
     */
    Mode toggleMode(Mode mode) noexcept;

    [[nodiscard]] bool allowsFileMenu() const noexcept { return _mode == Mode::Work; }
    [[nodiscard]] bool allowsGroupEdit() const noexcept { return _mode == Mode::Work; }
    [[nodiscard]] bool allowsBlockMode() const noexcept { return _mode != Mode::Show; }

    /** Нажатие лебёдки: Work/Show → mechSelect; Block → toggleMechBlocked. */
    [[nodiscard]] BlockResult pressMech(uint8_t id) noexcept;
    /**
     * Нажатие группы: Work/Show → recall / clear active;
     * Block → toggleGroupBlocked.
     */
    [[nodiscard]] BlockResult pressGroup(uint8_t id) noexcept;

    /* ========== Механизмы ========== */
    [[nodiscard]] Mech& mech(uint8_t id) noexcept { return _mechs[id]; }
    [[nodiscard]] const Mech& mech(uint8_t id) const noexcept { return _mechs[id]; }
    /** Есть ли Selected по телеметрии (не Mech::hasSelection / static). */
    [[nodiscard]] bool hasSelection() const noexcept;
    [[nodiscard]] bool isMechBlocked(uint8_t id) const noexcept;
    /**
     * Если лебёдка заблокирована — заполняет blockMessage() (ручная / группы) и true.
     * Иначе false, сообщение очищается.
     */
    [[nodiscard]] bool fillMechBlockMessage(uint8_t id) noexcept;
    /**
     * true: isolateGroup и есть active group, лебёдка не в ней
     * (UI — Disabled; выделение/блокировка не меняются).
     */
    [[nodiscard]] bool isMechIsolated(uint8_t id) const noexcept;

    /* ========== Группы ========== */
    [[nodiscard]] smcp::Group& group(uint8_t id) noexcept { return _groups[id]; }
    [[nodiscard]] const smcp::Group& group(uint8_t id) const noexcept { return _groups[id]; }
    [[nodiscard]] uint8_t getActiveGroup() const noexcept { return _activeGroup; }

    /* ========== Настройки ========== */
    [[nodiscard]] const Settings& settings() const noexcept { return _settings; }
    void setSettings(const Settings& settings) noexcept;

    /**
     * Записать выделение в группу.
     * @a confirmed false: занята → GroupOccupied + false; иначе запись.
     * @a confirmed true: перезапись (жёсткие проверки остаются).
     */
    [[nodiscard]] bool recordGroup(uint8_t id,
                                   const char* name = nullptr,
                                   bool confirmed = false) noexcept;
    [[nodiscard]] bool renameGroup(uint8_t id, const char* name) noexcept;
    /**
     * Очистить группу.
     * @a confirmed false: непуста → GroupOccupied + false; пустая → Ok.
     * @a confirmed true: очистка.
     */
    [[nodiscard]] bool clearGroup(uint8_t id, bool confirmed = false) noexcept;

    [[nodiscard]] const char* blockMessage() const noexcept { return _blockMsg; }

    /* ========== Шоуфайл ========== */
    [[nodiscard]] const char* showName() const noexcept { return _showName; }
    [[nodiscard]] bool isEdited() const noexcept { return _edited; }

    void newShow() noexcept;
    [[nodiscard]] bool openShow() noexcept;
    [[nodiscard]] bool saveShow() noexcept;
    /**
     * Save As по имени файла (не пути).
     * !confirmed && файл есть → BrowserFault + MBrowser::FileExists (имя запоминается);
     * confirmed / файла нет → запись. Повтор после Yes: saveShowAs(nullptr, true).
     */
    [[nodiscard]] bool saveShowAs(const char* name, bool confirmed = false) noexcept;

    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;
    /**
     * false + TemplateProtected, если @a name (или basename пути) — шаблон.
     * Иначе true и Status::Ok.
     */
    [[nodiscard]] bool canMutateShowName(const char* name) noexcept;

private:
    /* --- Статус --- */
    void clearError() noexcept { _status = Status::Ok; }

    /* --- Механизмы / SMCP --- */
    [[nodiscard]] static constexpr bool validMechId(uint8_t id) noexcept { return id < kMechCount; }
    [[nodiscard]] bool mechSelect(uint8_t id) noexcept;
    void clearSelection() noexcept;
    /** Маска групп (бит N — группа N), куда входит механизм и у группы есть биты из @a group_flags. */
    [[nodiscard]] uint64_t mechGroupMask(uint8_t mech_id,
                                         REG::BitMask<smcp::Group::Flag> group_flags) const noexcept;
    [[nodiscard]] smcp::Selection selectionFromMechs() const noexcept;
    void handlePacket(const smcp::msg::Packet& pkt) noexcept;
    void handleTelemetry(const smcp::msg::Header& hdr, const smcp::msg::Telemetry& body) noexcept;
    [[nodiscard]] uint8_t nextPktId() noexcept { return _pkt_tx++; }
    void requestFlush() noexcept;

    /* --- Группы --- */
    [[nodiscard]] static constexpr bool validGroupId(uint8_t id) noexcept
    {
        return id < smcp::kGroupMaxCount;
    }
    [[nodiscard]] bool recallGroup(uint8_t id) noexcept;
    void clearActiveGroup() noexcept;
    void initBlockedGroup() noexcept;
    [[nodiscard]] static bool groupsValid(const smcp::Group* groups, std::size_t count) noexcept;

    /* --- Блокировка --- */
    [[nodiscard]] BlockResult toggleMechBlocked(uint8_t id) noexcept;
    [[nodiscard]] BlockResult toggleGroupBlocked(uint8_t id) noexcept;
    /** Blocked=true → снять Selected с лебёдок группы и сбросить active, если это она. */
    void setGroupBlocked(uint8_t id, bool blocked) noexcept;
    /** Синхронизировать IMech::Status::Blocked с blocked-группами. */
    void rebuildBlockedMechs() noexcept;
    void clearBlockMessage() noexcept;
    /**
     * Id blocked-групп (id≥1), в которых есть @a mech_id.
     * @return число записанных id (≤ outCap).
     */
    [[nodiscard]] std::size_t collectBlockingGroupIds(uint8_t mech_id,
                                                      uint8_t* out,
                                                      std::size_t outCap) const noexcept;
    /**
     * Список id через запятую в @a out.
     * @a displayBias прибавляется к каждому id (лебёдки: +1).
     * @a mark префикс у каждого номера (например uiMsg::kWinchMark → «№1, №2»).
     */
    [[nodiscard]] static bool formatIdList(char* out,
                                           std::size_t outLen,
                                           const uint8_t* ids,
                                           std::size_t count,
                                           uint8_t displayBias = 0u,
                                           const char* mark = nullptr) noexcept;

    /* --- Шоуфайл (I/O) --- */
    void setShowName(const char* name) noexcept;
    void markEdited() noexcept { _edited = true; }
    void clearEdited() noexcept { _edited = false; }
    [[nodiscard]] static bool isTemplateName(const char* name) noexcept;
    /**
     * Импорт/экспорт SMCP через IFile (SD, зеркало, …).
     * openPath непустой → setShowName(openPath); пустой → Header::name.
     * Экспорт пишет _showName в Header::name.
     */
    [[nodiscard]] bool importShow(BIF::IFile& io, const char* openPath) noexcept;
    [[nodiscard]] bool exportShow(BIF::IFile& io, const char* openPath) noexcept;
    [[nodiscard]] bool saveShowPath(const char* path) noexcept;
    void persistMirror() noexcept;
    [[nodiscard]] static Status mapFileStatus(smcp::file::Status status) noexcept;

    /* --- Данные --- */
    MBrowser& _browser;
    BIF::IFile* _mirror = nullptr;
    FlushHook _flush = nullptr;
    uint32_t _telemetryDirty = 0;
    uint8_t _console_id;
    uint8_t _pkt_tx = 0;
    uint8_t _activeGroup = kBlockedGroupId;
    Mode _mode = Mode::Work;
    Status _status = Status::Ok;
    Settings _settings{};
    Mech _mechs[kMechCount];
    smcp::Group _groups[smcp::kGroupMaxCount];
    /** Кандидат GRUP при import/restore: validate → commit в _groups (текущее шоу не портим). */
    smcp::Group _groupsTmp[smcp::kGroupMaxCount];
    smcp::msg::PacketQueue<kRxDepth> _rx;
    smcp::msg::PacketQueue<kTxDepth> _tx;
    char _showName[smcp::file::kPathSize]{};
    char _saveAsName[BIF::kDirNameSize]{};
    char _blockMsg[160]{};
    bool _edited = false;
};
