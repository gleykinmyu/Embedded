/**
 * @file mconsole.hpp
 * @brief Модель консоли: режимы, механизмы, группы, шоуфайл (SETT пульта + GRUP).
 *
 * Держит ссылку на MBrowser: openShow / saveShow / saveShowAs работают через него.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "smcp/group.hpp"
#include "smcp/mech.hpp"
#include "smcp/show_file.hpp"

class MBrowser;

class MConsole {
public:
    /* ========== Константы ========== */
    static constexpr std::size_t kMechCount = 24u;
    static constexpr uint8_t kBlockedGroupId = 0u;
    /** Секция настроек пульта в шоуфайле (не SMCP) — FourCC "SETT". */
    static constexpr uint32_t kSettingsSectionTag = 0x54544553u;
    static constexpr std::size_t kSettingsWireSize = 8u;

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
                      uint8_t max_active_mechs = 3u,
                      uint8_t console_id = 1u) noexcept;

    /** Опциональное зеркало (любой IFile: W25Q-сектор, …). */
    void setMirror(BIF::IFile* mirror) noexcept { _mirror = mirror; }
    /** Boot: прочитать зеркало, если setMirror. */
    [[nodiscard]] bool restoreMirror() noexcept;

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
    [[nodiscard]] smcp::Mech& mech(uint8_t id) noexcept { return _mechs[id]; }
    [[nodiscard]] const smcp::Mech& mech(uint8_t id) const noexcept { return _mechs[id]; }
    [[nodiscard]] bool isMechBlocked(uint8_t id) const noexcept;
    /**
     * true: isolateGroup и есть active group, лебёдка не в ней
     * (UI — Disabled; выделение/блокировка не меняются).
     */
    [[nodiscard]] bool isMechIsolated(uint8_t id) const noexcept;
    [[nodiscard]] std::size_t selectedCount() const noexcept;

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

    /* --- Механизмы --- */
    [[nodiscard]] static constexpr bool validMechId(uint8_t id) noexcept { return id < kMechCount; }
    [[nodiscard]] bool mechSelect(uint8_t id) noexcept;
    void clearSelection() noexcept;
    /** Маска групп (бит N — группа N), куда входит механизм и у группы есть биты из @a group_flags. */
    [[nodiscard]] uint64_t mechGroupMask(uint8_t mech_id,
                                         REG::BitMask<smcp::Group::Flag> group_flags) const noexcept;
    [[nodiscard]] smcp::Selection selectionFromMechs() const noexcept;

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
    void clearBlockMessage() noexcept;
    /**
     * Список id через запятую в @a out.
     * @a displayBias прибавляется к каждому id (лебёдки: +1 для нумерации с 1 на экране).
     */
    [[nodiscard]] static bool formatIdList(char* out,
                                           std::size_t outLen,
                                           const uint8_t* ids,
                                           std::size_t count,
                                           uint8_t displayBias = 0u) noexcept;

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
    uint8_t _console_id;
    uint8_t _maxActiveMechs;
    uint8_t _activeGroup = kBlockedGroupId;
    Mode _mode = Mode::Work;
    Status _status = Status::Ok;
    Settings _settings{};
    smcp::Mech _mechs[kMechCount];
    smcp::Group _groups[smcp::kGroupMaxCount];
    char _showName[smcp::file::kPathSize]{};
    char _saveAsName[BIF::kDirNameSize]{};
    char _blockMsg[160]{};
    bool _edited = false;
};
