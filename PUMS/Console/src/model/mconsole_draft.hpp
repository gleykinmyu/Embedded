/**
 * @file mconsole_draft.hpp
 * @brief Черновик CLI-поверхности MConsole (не подключать из UI / реального MConsole).
 *
 * Правь имена, сигнатуры, добавляй методы. Потом перенесём в MConsole.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace Test {

class MConsole {
public:
    /* ========== Типы ========== */

    /** Режим консоли. Block — не режим, а команда blockMech / blockGroup. */
    enum class Mode : uint8_t {
        Work = 0,
        Show = 1,
    };

    enum class Status : uint8_t {
        Ok = 0,
        /* шоуфайл / носитель */
        NoShowOpen,
        TemplateProtected,
        BadMagic,
        BadVersion,
        BadHeaderCrc,
        BadBodyCrc,
        BadLayout,
        Truncated,
        MissingGrup,
        BadGroups,
        IoError,
        /* группы */
        InvalidGroup,
        NoSelection,
        GroupOccupied,
        /* файлы */
        BrowserFault,
        InvalidName,
        NotFound,
        FileExists,
    };

    enum class Result : uint8_t {
        NoChange = 0,
        Changed,
        Rejected,
        Warning, /**< Текст — lastMessage(). */
    };

    struct Settings {
        bool isolateGroup = false;
        uint8_t reserved[7]{};
    };

    static constexpr uint8_t kNoActiveGroup = 0xFFu;
    static constexpr uint8_t kMechCount = 24u;
    static constexpr uint8_t kGroupCount = 32u;

    /* ========== Режим ========== */

    /** `mode` */
    [[nodiscard]] Mode getMode() const noexcept;

    /** `mode work` / `mode show` */
    void setMode(Mode mode) noexcept;

    /* ========== Лебёдки: команды ========== */

    /**
     * `select <id>` / `deselect <id>`
     * @a on true — выбрать; false — снять. Уже в нужном состоянии → NoChange.
     */
    [[nodiscard]] Result selectMech(uint8_t id, bool on = true) noexcept;

    /**
     * `block <id>` / `unblock <id>`
     * Сегментный Block на сервер. @a on — целевое состояние, не toggle.
     */
    [[nodiscard]] Result blockMech(uint8_t id, bool on = true) noexcept;

    /* ========== Лебёдки: запросы ========== */

    /** `has-selection` */
    [[nodiscard]] bool hasSelection() const noexcept;

    /** `mech <id> selected` */
    [[nodiscard]] bool isMechSelected(uint8_t id) const noexcept;

    /** `mech <id> ready` */
    [[nodiscard]] bool isMechReady(uint8_t id) const noexcept;

    /** Сегментный Block (сервер / телеметрия). */
    [[nodiscard]] bool isMechServerBlocked(uint8_t id) const noexcept;

    /** Лебёдка входит в GRUP с Flag::Blocked. */
    [[nodiscard]] bool isMechGroupBlocked(uint8_t id) const noexcept;

    /** server || GRUP */
    [[nodiscard]] bool isMechBlocked(uint8_t id) const noexcept;

    /**
     * isolateGroup + есть active group + лебёдка не в ней.
     * Выделение/железо не меняются — только факт «чужая».
     */
    [[nodiscard]] bool isMechIsolated(uint8_t id) const noexcept;

    /**
     * `why-blocked <id>`
     * true → текст в lastMessage(); false → сообщения нет.
     */
    [[nodiscard]] bool fillMechBlockMessage(uint8_t id) noexcept;

    /* ========== Группы: команды ========== */

    /** `recall <id>` */
    [[nodiscard]] Result recallGroup(uint8_t id) noexcept;

    /** `unrecall` — снять активную группу / selection. */
    void clearActiveGroup() noexcept;

    /**
     * `record <id> [name] [--force]`
     * !confirmed && занята → GroupOccupied + false.
     */
    [[nodiscard]] bool recordGroup(uint8_t id,
                                   const char* name = nullptr,
                                   bool confirmed = false) noexcept;

    /** `rename <id> <name>` */
    [[nodiscard]] bool renameGroup(uint8_t id, const char* name) noexcept;

    /**
     * `clear <id> [--force]`
     * !confirmed && непуста → GroupOccupied + false.
     */
    [[nodiscard]] bool clearGroup(uint8_t id, bool confirmed = false) noexcept;

    /**
     * `block-group <id>` / `unblock-group <id>`
     * GRUP Flag::Blocked. @a on — целевое состояние, не toggle.
     */
    [[nodiscard]] Result blockGroup(uint8_t id, bool on = true) noexcept;

    /* ========== Группы: запросы ========== */

    /** Подтверждённый active (Ack). isolate смотрит сюда. */
    [[nodiscard]] uint8_t getActiveGroup() const noexcept;

    /** Цель recall/clear, пока Select в полёте; иначе = getActiveGroup(). */
    [[nodiscard]] uint8_t pendingActiveGroup() const noexcept;

    [[nodiscard]] bool isGroupEmpty(uint8_t id) const noexcept;
    [[nodiscard]] bool isGroupBlocked(uint8_t id) const noexcept;
    [[nodiscard]] const char* groupName(uint8_t id) const noexcept;

    /** Число членов; ids — опционально, не больше outCap. */
    [[nodiscard]] uint8_t groupMembers(uint8_t id,
                                       uint8_t* out = nullptr,
                                       uint8_t outCap = 0u) const noexcept;

    /* ========== Шоуфайл: команды ========== */

    /** `new` */
    void newShow() noexcept;

    /** `open <name>` — имя файла, не полный путь. */
    [[nodiscard]] bool openShow(const char* name) noexcept;

    /** `save` */
    [[nodiscard]] bool saveShow() noexcept;

    /**
     * `save-as <name> [--force]`
     * !confirmed && файл есть → FileExists + false.
     */
    [[nodiscard]] bool saveShowAs(const char* name, bool confirmed = false) noexcept;

    /** `rm <name>` — нельзя удалить открытый. */
    [[nodiscard]] bool removeShow(const char* name) noexcept;

    /* ========== Шоуфайл: запросы ========== */

    [[nodiscard]] const char* showName() const noexcept;
    [[nodiscard]] bool isEdited() const noexcept;

    /** basename пути / шаблон? */
    [[nodiscard]] static const char* showBaseName(const char* path) noexcept;
    [[nodiscard]] bool isTemplateName(const char* name) const noexcept;

    /**
     * `ls [from] [count]`
     * Пишет до @a count имён начиная с @a from.
     * @return сколько записано; имена — указатели в кэш тома (до следующего ls/refresh).
     */
    [[nodiscard]] std::size_t getFileNames(std::size_t from,
                                           std::size_t count,
                                           const char** out) noexcept;

    [[nodiscard]] std::size_t fileCount() const noexcept;

    /* ========== Настройки / зеркало ========== */

    /** `settings` */
    [[nodiscard]] const Settings& settings() const noexcept;

    /** `settings isolate on|off` */
    void setSettings(const Settings& settings) noexcept;

    /** `restore-mirror` */
    [[nodiscard]] bool restoreMirror() noexcept;

    /* ========== Статус последней команды ========== */

    [[nodiscard]] Status getStatus() const noexcept;
    [[nodiscard]] static const char* statusText(Status status) noexcept;

    /** Текст Warning / why-blocked. */
    [[nodiscard]] const char* lastMessage() const noexcept;

    [[nodiscard]] uint8_t lastNackCode() const noexcept; /**< smcp::msg::ErrorCode как uint8_t, пока без smcp. */
    [[nodiscard]] uint8_t lastNackReq() const noexcept;
    [[nodiscard]] uint8_t lastNackDetail() const noexcept; /**< mech_id или none */
    [[nodiscard]] static const char* nackText(uint8_t code) noexcept;

    /* ========== События (watch) ========== */

    /** Телеметрия лебёдки изменилась. */
    void setOnMechChanged(void (*fn)(uint8_t id) noexcept) noexcept;

    /** Nack / Timeout на Select|Block|SetTarget. Код — lastNack*. */
    void setOnNack(void (*fn)() noexcept) noexcept;

    /** Ack Select (recall / unrecall / selectMech) — active group закоммичен. */
    void setOnSelectAck(void (*fn)() noexcept) noexcept;

    /** Фаза линка (smcp IConsole::Phase). */
    void setOnPhase(void (*fn)(uint8_t phase) noexcept) noexcept;

    /** Имя шоу / флаг edited. */
    void setOnShowChanged(void (*fn)() noexcept) noexcept;

    /** Модель изменилась (режим, группы, settings) — не telemetry и не Select Ack. */
    void setOnChanged(void (*fn)() noexcept) noexcept;
};

} // namespace Test
