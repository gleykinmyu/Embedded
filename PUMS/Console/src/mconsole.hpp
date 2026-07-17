/**
 * @file mconsole.hpp
 * @brief Модель консоли: выделение механизмов, группы, шоуфайл (секция GRUP).
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
    static constexpr std::size_t kMechCount = 24u;
    static constexpr uint8_t kBlockedGroupId = 0u;

    explicit MConsole(uint8_t max_active_mechs = 3u, uint8_t console_id = 1u) noexcept;

    enum class Mode : uint8_t {
        Work = 0,
        Block = 1,
        Show = 2,
    };

    /** Результат блокировки лебёдки/группы в Mode::Block. */
    enum class BlockResult : uint8_t {
        NoChange = 0,
        Changed,
        /** Ручная блокировка лебёдки запрещена (есть blocked-группы). */
        Rejected,
        /** Группа заблокирована, но есть пересечение мехов — см. blockMessage(). */
        Warning,
    };

    [[nodiscard]] uint8_t consoleId() const noexcept { return _console_id; }

    [[nodiscard]] uint8_t maxActiveMechs() const noexcept { return _maxActiveMechs; }

    void setMaxActiveMechs(uint8_t limit) noexcept { _maxActiveMechs = limit; }

    [[nodiscard]] Mode mode() const noexcept { return _mode; }

    /** Установить режим (взаимоисключающие Work / Block / Show). */
    void setMode(Mode mode) noexcept;

    /**
     * Переключить режим-кнопку: повторный вызов того же режима → Work,
     * иначе → @a mode. Work как аргумент всегда ставит Work.
     */
    Mode toggleMode(Mode mode) noexcept;

    /** Меню файла / assign групп — только в Work. */
    [[nodiscard]] bool allowsFileMenu() const noexcept { return _mode == Mode::Work; }
    [[nodiscard]] bool allowsGroupEdit() const noexcept { return _mode == Mode::Work; }
    /** Кнопка Block недоступна в режиме Show. */
    [[nodiscard]] bool allowsBlockMode() const noexcept { return _mode != Mode::Show; }

    /** Нажатие лебёдки: Work/Show → select; Block → toggle blocked. */
    [[nodiscard]] BlockResult pressMech(uint8_t id) noexcept;

    /**
     * Нажатие группы: Work/Show → recall / clear active;
     * Block → toggle Group::Flag::Blocked.
     */
    [[nodiscard]] BlockResult pressGroup(uint8_t id) noexcept;

    [[nodiscard]] bool isMechBlocked(uint8_t id) const noexcept;

    /**
     * Вкл/выкл механизм в группе Blocked (id 0).
     * При включении: если лебёдка уже в заблокированной пользовательской группе —
     * Rejected + blockMessage() со списком номеров групп.
     */
    [[nodiscard]] BlockResult toggleMechBlocked(uint8_t id) noexcept;

    /**
     * Вкл/выкл флаг Blocked у пользовательской группы.
     * При включении: пересечение мехов с уже заблокированными группами →
     * всё равно блокируем, но Warning + blockMessage() со списком лебёдок.
     */
    [[nodiscard]] BlockResult toggleGroupBlocked(uint8_t id) noexcept;

    /** Текст последней ошибки/предупреждения блокировки (никогда не nullptr). */
    [[nodiscard]] const char* blockMessage() const noexcept { return _blockMsg; }

    /** Переключить механизм в выделении; true — состояние изменилось. */
    [[nodiscard]] bool select(uint8_t id) noexcept;

    void clearSelection() noexcept;

    [[nodiscard]] smcp::Mech& mech(uint8_t id) noexcept { return _mechs[id]; }

    [[nodiscard]] const smcp::Mech& mech(uint8_t id) const noexcept { return _mechs[id]; }

    [[nodiscard]] std::size_t mechCount() const noexcept { return kMechCount; }

    /** Маска групп (бит N — группа N), куда входит механизм и у группы есть биты из @a group_flags. */
    [[nodiscard]] uint64_t mechGroupMask(uint8_t mech_id,
                                         REG::BitMask<smcp::Group::Flag> group_flags) const noexcept;

    /** Создать группу из текущего выделения. */
    [[nodiscard]] bool createGroup(uint8_t id, const char* name = nullptr) noexcept;

    [[nodiscard]] bool renameGroup(uint8_t id, const char* name) noexcept;

    [[nodiscard]] bool clearGroup(uint8_t id) noexcept;

    /** Восстановить выделение механизмов из группы. */
    [[nodiscard]] bool recallGroup(uint8_t id) noexcept;

    /** Снять выделение и сбросить активную группу. */
    void clearActiveGroup() noexcept;

    [[nodiscard]] uint8_t getActiveGroup() const noexcept { return _activeGroup; }

    [[nodiscard]] smcp::Group& group(uint8_t id) noexcept { return _groups[id]; }

    [[nodiscard]] const smcp::Group& group(uint8_t id) const noexcept { return _groups[id]; }

    [[nodiscard]] std::size_t groupCount() const noexcept { return smcp::kGroupMaxCount; }

    /** Пустой шоу: сброс всех групп и выделения. */
    void newShow() noexcept;

    enum class Status : uint8_t {
        Ok = 0,
        /** Пустой путь / нет открытого шоу. */
        NoShowOpen,
        /** Имя файла — шаблон (нельзя сохранять). */
        TemplateProtected,
        /** Неверная сигнатура SMCP. */
        BadMagic,
        /** Несовместимая версия шоуфайла. */
        BadVersion,
        /** Не совпал CRC заголовка. */
        BadHeaderCrc,
        /** Не совпал CRC тела. */
        BadBodyCrc,
        /** Повреждённая структура / каталог. */
        BadLayout,
        /** Файл короче заявленного размера. */
        Truncated,
        /** Нет секции GRUP. */
        MissingGrup,
        /** Некорректные записи групп. */
        BadGroups,
        /** Ошибка носителя (open/read/write). */
        IoError,
    };

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::Ok; }

    /** Текст для MsgBox (никогда не nullptr). */
    [[nodiscard]] static const char* statusText(Status status) noexcept;

    /** Базовое имя из пути (`dir/file.show` → `file.show`). */
    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;

    /** Имя/базовое имя — защищённый шаблон (нельзя save/delete). */
    [[nodiscard]] static bool isTemplateName(const char* name) noexcept;

    /**
     * false + TemplateProtected, если @a name (или basename пути) — шаблон.
     * Иначе true и Status::Ok.
     */
    [[nodiscard]] bool canMutateShowName(const char* name) noexcept;

    [[nodiscard]] bool loadShow(MBrowser& browser, const char* path) noexcept;

    [[nodiscard]] bool saveShow(MBrowser& browser, const char* path) noexcept;

    [[nodiscard]] const char* showName() const noexcept { return _showName; }

    void setShowName(const char* name) noexcept;

    /** Есть несохранённые изменения шоу в памяти. */
    [[nodiscard]] bool isEdited() const noexcept { return _edited; }
    void markEdited() noexcept { _edited = true; }
    void clearEdited() noexcept { _edited = false; }

private:
    [[nodiscard]] static constexpr bool validMechId(uint8_t id) noexcept { return id < kMechCount; }

    [[nodiscard]] static constexpr bool validGroupId(uint8_t id) noexcept
    {
        return id < smcp::kGroupMaxCount;
    }

    [[nodiscard]] std::size_t selectedCount() const noexcept;

    [[nodiscard]] smcp::Selection selectionFromMechs() const noexcept;

    void initBlockedGroup() noexcept;

    [[nodiscard]] static bool groupsValid(const smcp::Group* groups, std::size_t count) noexcept;
    [[nodiscard]] static Status mapFileStatus(smcp::file::Status status) noexcept;

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

    uint8_t _console_id;
    uint8_t _maxActiveMechs;
    uint8_t _activeGroup = kBlockedGroupId;
    Mode _mode = Mode::Work;
    Status _status = Status::Ok;
    smcp::Mech _mechs[kMechCount];
    smcp::Group _groups[smcp::kGroupMaxCount];
    char _showName[smcp::file::kShowFileNameSize]{};
    char _blockMsg[160]{};
    bool _edited = false;
};
