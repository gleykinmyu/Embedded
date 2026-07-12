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

class MConsole {
public:
    static constexpr std::size_t kMechCount = 24u;
    static constexpr uint8_t kBlockedGroupId = 0u;

    explicit MConsole(uint8_t max_active_mechs = 3u, uint8_t console_id = 1u) noexcept;

    [[nodiscard]] uint8_t consoleId() const noexcept { return _console_id; }

    [[nodiscard]] uint8_t maxActiveMechs() const noexcept { return _maxActiveMechs; }

    void setMaxActiveMechs(uint8_t limit) noexcept { _maxActiveMechs = limit; }

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

    [[nodiscard]] smcp::Group& group(uint8_t id) noexcept { return _groups[id]; }

    [[nodiscard]] const smcp::Group& group(uint8_t id) const noexcept { return _groups[id]; }

    [[nodiscard]] std::size_t groupCount() const noexcept { return smcp::kGroupMaxCount; }

    /** Пустой шоу: сброс всех групп и выделения. */
    void newShow() noexcept;

    [[nodiscard]] bool loadShow(smcp::file::IFile& io, const char* path) noexcept;

    [[nodiscard]] bool saveShow(smcp::file::IFile& io, const char* path) noexcept;

    [[nodiscard]] const char* showName() const noexcept { return _showName; }

    void setShowName(const char* name) noexcept;

private:
    [[nodiscard]] static constexpr bool validMechId(uint8_t id) noexcept { return id < kMechCount; }

    [[nodiscard]] static constexpr bool validGroupId(uint8_t id) noexcept
    {
        return id < smcp::kGroupMaxCount;
    }

    [[nodiscard]] std::size_t selectedCount() const noexcept;

    [[nodiscard]] smcp::Selection selectionFromMechs() const noexcept;

    void initBlockedGroup() noexcept;

    uint8_t _console_id;
    uint8_t _maxActiveMechs;
    smcp::Mech _mechs[kMechCount];
    smcp::Group _groups[smcp::kGroupMaxCount];
    char _showName[smcp::file::kShowFileNameSize]{};
};
