/**
 * @file mconsole.hpp
 * @brief Модель консоли проекта: режимы, группы, шоуфайл поверх smcp::Console<N>.
 *
 * Inventory CMech* — в базе; объекты — CMechBank (register через CMech ctor).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "smcp/Console/console.hpp"
#include "smcp/Console/cmech.hpp"
#include "smcp/Console/show_file.hpp"
#include "smcp/Console/group.hpp"
#include "smcp/transport/ilink.hpp"

class MBrowser;

class MConsole : public smcp::Console<24> {
public:
    /* ========== Константы ========== */
    using Console::kMechCount;
    /** Нет активной группы (не id слота GRUP). */
    static constexpr uint8_t kNoActiveGroup = 0xFFu;
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
        Rejected, /**< Зарезервировано / отказ операции блокировки. */
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
        InvalidGroup,       /**< Неверный id группы. */
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
                      smcp::ILink& link,
                      smcp::Node::ClockFn clock) noexcept;

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

    /**
     * Нажатие лебёдки: Work/Show → mechSelect;
     * Block → сегментный Block на сервер (не GRUP).
     */
    [[nodiscard]] BlockResult pressMech(uint8_t id) noexcept;
    /**
     * Нажатие группы: Work/Show → recall / clear active;
     * Block → GRUP Flag::Blocked в шоуфайле.
     */
    [[nodiscard]] BlockResult pressGroup(uint8_t id) noexcept;

    /* ========== Механизмы ========== */
    [[nodiscard]] smcp::CMech& mech(uint8_t id) noexcept { return _cmechs[id]; }
    [[nodiscard]] const smcp::CMech& mech(uint8_t id) const noexcept { return _cmechs[id]; }
    /** Есть ли Selected по телеметрии. */
    [[nodiscard]] bool hasSelection() const noexcept;
    /** Сегментный Blocked (Telemetry / IMech). Приоритет над GRUP. */
    [[nodiscard]] bool isMechServerBlocked(uint8_t id) const noexcept;
    /** GRUP blocked (Flag::Blocked в шоуфайле). */
    [[nodiscard]] bool isMechGroupBlocked(uint8_t id) const noexcept;
    /** Любая блокировка: server || GRUP. */
    [[nodiscard]] bool isMechBlocked(uint8_t id) const noexcept;
    /**
     * Если лебёдка заблокирована — blockMessage() (server / GRUP) и true.
     * Иначе false, сообщение очищается. Текст: сначала server.
     */
    [[nodiscard]] bool fillMechBlockMessage(uint8_t id) noexcept;
    /** Edge Telemetry → UI (mech_id). */
    void setOnMechChanged(void (*fn)(uint8_t) noexcept) noexcept { _onMechChanged = fn; }
    /**
     * Edge Nack / Timeout на Select|Block|SetTarget.
     * Код — lastNack(); текст — nackText().
     */
    void setOnNack(void (*fn)() noexcept) noexcept { _onNack = fn; }
    /** Ack Select (recall/clear/mech) → UI isolate refresh. */
    void setOnSelectAck(void (*fn)() noexcept) noexcept { _onSelectAck = fn; }
    /** Edge IConsole::Phase → UI. */
    void setOnPhase(void (*fn)(smcp::IConsole::Phase) noexcept) noexcept { _onPhase = fn; }
    [[nodiscard]] smcp::msg::ErrorCode lastNack() const noexcept { return _lastNack; }
    [[nodiscard]] smcp::msg::MsgId lastNackReq() const noexcept { return _lastNackReq; }
    /** mech_id или kNackDetailNone. */
    [[nodiscard]] uint8_t lastNackDetail() const noexcept { return _lastNackDetail; }
    [[nodiscard]] static const char* nackText(smcp::msg::ErrorCode code) noexcept;
    /**
     * true: isolateGroup и есть active group, лебёдка не в ней
     * (UI — Disabled; выделение/блокировка не меняются).
     */
    [[nodiscard]] bool isMechIsolated(uint8_t id) const noexcept;

    /* ========== Группы ========== */
    [[nodiscard]] smcp::Group& group(uint8_t id) noexcept { return _groups[id]; }
    [[nodiscard]] const smcp::Group& group(uint8_t id) const noexcept { return _groups[id]; }
    [[nodiscard]] uint8_t getActiveGroup() const noexcept { return _activeGroup; }
    /**
     * Для подсветки кнопок групп: pending Select ещё в полёте → цель,
     * иначе подтверждённый active (isolate смотрит только getActiveGroup).
     */
    [[nodiscard]] uint8_t uiActiveGroup() const noexcept
    {
        return _groupSelectPending ? _pendingActiveGroup : _activeGroup;
    }

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
    void setOnShowChanged(void (*fn)() noexcept) noexcept { _onShowChanged = fn; }

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
    [[nodiscard]] static bool groupsValid(const smcp::Group* groups, uint8_t count) noexcept;

    /* --- SMCP leaf --- */
    void onTelemetry(const smcp::msg::Header& hdr,
                     const smcp::msg::Telemetry& body) noexcept override;
    void onAck(smcp::Session* session, const smcp::TxSlot& req) noexcept override;
    void onNack(smcp::Session* session, const smcp::TxSlot& req,
                const smcp::msg::Nack& reply) noexcept override;
    void onPhase(Phase phase) noexcept override;

    /* --- Блокировка --- */
    /** Сегментный Block TX (CMech::block). */
    [[nodiscard]] BlockResult toggleMechServerBlocked(uint8_t id) noexcept;
    /** GRUP Flag::Blocked в шоуфайле. */
    [[nodiscard]] BlockResult toggleGroupBlocked(uint8_t id) noexcept;
    /** Blocked=true → снять Selected с лебёдок группы и сбросить active, если это она. */
    void setGroupBlocked(uint8_t id, bool blocked) noexcept;
    void clearBlockMessage() noexcept;
    /**
     * Id blocked-групп, в которых есть @a mech_id.
     * @return число записанных id (≤ outCap).
     */
    [[nodiscard]] uint8_t collectBlockingGroupIds(uint8_t mech_id,
                                                      uint8_t* out,
                                                      uint8_t outCap) const noexcept;
    /**
     * Список id через запятую в @a out.
     * @a displayBias прибавляется к каждому id (лебёдки: +1).
     * @a mark префикс у каждого номера (например uiMsg::kWinchMark → «№1, №2»).
     */
    [[nodiscard]] static bool formatIdList(char* out,
                                           std::size_t outLen,
                                           const uint8_t* ids,
                                           uint8_t count,
                                           uint8_t displayBias = 0u,
                                           const char* mark = nullptr) noexcept;

    /* --- Шоуфайл (I/O) --- */
    void setShowName(const char* name) noexcept;
    void markEdited() noexcept;
    void clearEdited() noexcept;
    void notifyShowChanged() noexcept;
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
    /** CMech bank; primary Session — в Console. */
    smcp::CMechBank<kMechCount> _cmechs;
    MBrowser& _browser;
    BIF::IFile* _mirror = nullptr;
    uint8_t _activeGroup = kNoActiveGroup;       /**< Подтверждённый (Ack); isolate. */
    uint8_t _pendingActiveGroup = kNoActiveGroup; /**< Цель recall/clear до Ack/Nack. */
    bool _groupSelectPending = false;
    Mode _mode = Mode::Work;
    Status _status = Status::Ok;
    Settings _settings{};
    smcp::Group _groups[smcp::kGroupMaxCount];
    char _showName[smcp::file::kPathSize]{};
    char _saveAsName[BIF::kDirNameSize]{};
    char _blockMsg[160]{};
    bool _edited = false;
    void (*_onShowChanged)() noexcept = nullptr;
    void (*_onMechChanged)(uint8_t) noexcept = nullptr;
    void (*_onNack)() noexcept = nullptr;
    void (*_onSelectAck)() noexcept = nullptr;
    void (*_onPhase)(Phase) noexcept = nullptr;
    smcp::msg::ErrorCode _lastNack = smcp::msg::ErrorCode::Ok;
    smcp::msg::MsgId _lastNackReq = smcp::msg::MsgId::Select;
    uint8_t _lastNackDetail = smcp::msg::kNackDetailNone;
};
