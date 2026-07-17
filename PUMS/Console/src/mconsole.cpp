/**
 * @file mconsole.cpp
 * @brief Реализация MConsole.
 */

#include "mconsole.hpp"

#include "mbrowser.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char kDefaultGroupName[] = "Group";
constexpr const char kBlockedGroupName[] = "Blocked";

} // namespace

const char* MConsole::showBaseName(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        return "";
    }
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

bool MConsole::isTemplateName(const char* name) noexcept
{
    static constexpr char kTemplate[] = {
        static_cast<char>(0xFB), static_cast<char>(0xE1), static_cast<char>(0xE2),
        static_cast<char>(0xEC), static_cast<char>(0xEF), static_cast<char>(0xEE),
        '\0',
    };
    return name != nullptr && std::strncmp(name, kTemplate, sizeof(kTemplate) - 1u) == 0;
}

bool MConsole::canMutateShowName(const char* name) noexcept
{
    clearError();
    if (isTemplateName(showBaseName(name))) {
        _status = Status::TemplateProtected;
        return false;
    }
    _status = Status::Ok;
    return true;
}

bool MConsole::groupsValid(const smcp::Group* groups, std::size_t count) noexcept
{
    if (groups == nullptr || count == 0u || count > smcp::kGroupMaxCount) {
        return false;
    }

    constexpr uint8_t kKnownFlags = static_cast<uint8_t>(smcp::Group::Flag::Blocked)
        | static_cast<uint8_t>(smcp::Group::Flag::Atomic);

    for (std::size_t i = 0; i < count; ++i) {
        const smcp::Group& g = groups[i];
        if ((g.flag.raw() & static_cast<uint8_t>(~kKnownFlags)) != 0u) {
            return false;
        }
        if (std::memchr(g.name, '\0', smcp::kGroupNameSize) == nullptr) {
            return false;
        }
    }
    return true;
}

MConsole::Status MConsole::mapFileStatus(smcp::file::Status status) noexcept
{
    switch (status) {
    case smcp::file::Status::Ok:
        return Status::Ok;
    case smcp::file::Status::BadMagic:
        return Status::BadMagic;
    case smcp::file::Status::BadVersion:
        return Status::BadVersion;
    case smcp::file::Status::BadHeaderCrc:
        return Status::BadHeaderCrc;
    case smcp::file::Status::BadBodyCrc:
        return Status::BadBodyCrc;
    case smcp::file::Status::BadLayout:
        return Status::BadLayout;
    case smcp::file::Status::Truncated:
        return Status::Truncated;
    case smcp::file::Status::IoError:
    default:
        return Status::IoError;
    }
}

const char* MConsole::statusText(Status status) noexcept
{
    switch (status) {
    case Status::Ok:
        return "OK";
    case Status::NoShowOpen:
        return "No file open. Use Save As.";
    case Status::TemplateProtected:
        return "Cannot save template.";
    case Status::BadMagic:
        return "Invalid show file.";
    case Status::BadVersion:
        return "Unsupported file version.";
    case Status::BadHeaderCrc:
    case Status::BadBodyCrc:
        return "File checksum error.";
    case Status::BadLayout:
        return "Corrupt show file.";
    case Status::Truncated:
        return "Incomplete show file.";
    case Status::MissingGrup:
        return "Show file has no groups.";
    case Status::BadGroups:
        return "Invalid group data.";
    case Status::IoError:
    default:
        return "Storage error.";
    }
}

void MConsole::initBlockedGroup() noexcept
{
    smcp::Group& grp = _groups[kBlockedGroupId];
    grp.clear();
    grp.id = kBlockedGroupId;
    grp.setBlocked(true);
    grp.setName(kBlockedGroupName);
}

MConsole::MConsole(uint8_t max_active_mechs, uint8_t console_id) noexcept
    : _console_id(console_id)
    , _maxActiveMechs(max_active_mechs)
{
    for (std::size_t i = 0; i < kMechCount; ++i) {
        _mechs[i] = smcp::Mech(static_cast<uint8_t>(i));
    }

    for (std::size_t i = 0; i < smcp::kGroupMaxCount; ++i) {
        _groups[i].id = static_cast<uint8_t>(i);
        _groups[i].clear();
    }

    initBlockedGroup();

    _showName[0] = '\0';
}

std::size_t MConsole::selectedCount() const noexcept
{
    std::size_t count = 0;
    for (const smcp::Mech& mech : _mechs) {
        if (mech.status().any(smcp::IMech::Status::Selected)) {
            ++count;
        }
    }
    return count;
}

smcp::Selection MConsole::selectionFromMechs() const noexcept
{
    smcp::Selection selection;
    for (std::size_t i = 0; i < kMechCount; ++i) {
        if (_mechs[i].status().any(smcp::IMech::Status::Selected)) {
            selection.add(static_cast<uint8_t>(i));
        }
    }
    return selection;
}

uint64_t MConsole::mechGroupMask(uint8_t mech_id, REG::BitMask<smcp::Group::Flag> group_flags) const noexcept
{
    if (!validMechId(mech_id)) {
        return 0ULL;
    }

    uint64_t mask = 0ULL;
    for (std::size_t i = 0; i < smcp::kGroupMaxCount; ++i) {
        const smcp::Group& grp = _groups[i];
        if (grp.flag.all(group_flags) && grp.mech.contains(mech_id)) {
            mask |= (1ULL << i);
        }
    }
    return mask;
}

void MConsole::clearSelection() noexcept
{
    for (smcp::Mech& mech : _mechs) {
        if (mech.status().any(smcp::IMech::Status::Selected)) {
            mech.select(smcp::kSelectOwnerNone);
        }
    }
}

bool MConsole::select(uint8_t id) noexcept
{
    if (!validMechId(id) || mechGroupMask(id, smcp::Group::Flag::Blocked) != 0ULL) {
        return false;
    }

    smcp::Mech& mech = _mechs[id];

    if (mech.status().any(smcp::IMech::Status::Selected)) {
        return mech.select(smcp::kSelectOwnerNone);
    }

    if (selectedCount() >= _maxActiveMechs) {
        return false;
    }

    return mech.select(_console_id);
}

bool MConsole::createGroup(uint8_t id, const char* name) noexcept
{
    const smcp::Selection selection = selectionFromMechs();
    if (!validGroupId(id) || selection.empty()) {
        return false;
    }

    smcp::Group& grp = _groups[id];
    const bool overwrite = !grp.isEmpty();
    grp.id = id;
    grp.setSelection(selection);

    if (id == kBlockedGroupId) {
        grp.setBlocked(true);
        grp.setName(kBlockedGroupName);
        markEdited();
        return true;
    }

    if (name != nullptr && name[0] != '\0') {
        grp.setName(name);
    } else if (!overwrite) {
        grp.setName(kDefaultGroupName);
    }

    markEdited();
    return true;
}

bool MConsole::renameGroup(uint8_t id, const char* name) noexcept
{
    if (!validGroupId(id) || id == kBlockedGroupId || name == nullptr || name[0] == '\0') {
        return false;
    }

    if (_groups[id].isEmpty()) {
        return false;
    }

    _groups[id].setName(name);
    markEdited();
    return true;
}

bool MConsole::clearGroup(uint8_t id) noexcept
{
    if (!validGroupId(id)) {
        return false;
    }

    if (id == kBlockedGroupId) {
        _groups[id].mech = smcp::Selection{};
        markEdited();
        return true;
    }

    _groups[id].clear();
    markEdited();
    return true;
}

bool MConsole::recallGroup(uint8_t id) noexcept
{
    if (!validGroupId(id) || _groups[id].isEmpty()) {
        return false;
    }

    clearSelection();

    const smcp::Selection& group_sel = _groups[id].mech;
    for (std::size_t i = 0; i < kMechCount; ++i) {
        if (group_sel.contains(static_cast<uint8_t>(i))) {
            _mechs[i].select(_console_id);
        }
    }

    _activeGroup = id;
    return true;
}

void MConsole::clearActiveGroup() noexcept
{
    clearSelection();
    _activeGroup = kBlockedGroupId;
}

void MConsole::setMode(Mode mode) noexcept
{
    _mode = mode;
}

MConsole::Mode MConsole::toggleMode(Mode mode) noexcept
{
    if (mode == Mode::Work || _mode == mode) {
        _mode = Mode::Work;
    } else {
        _mode = mode;
    }
    return _mode;
}

bool MConsole::isMechBlocked(uint8_t id) const noexcept
{
    return validMechId(id) && mechGroupMask(id, smcp::Group::Flag::Blocked) != 0ULL;
}

void MConsole::clearBlockMessage() noexcept
{
    _blockMsg[0] = '\0';
}

bool MConsole::formatIdList(char* out,
                            std::size_t outLen,
                            const uint8_t* ids,
                            std::size_t count,
                            uint8_t displayBias) noexcept
{
    if (out == nullptr || outLen == 0u) {
        return false;
    }
    out[0] = '\0';
    if (ids == nullptr || count == 0u) {
        return false;
    }

    std::size_t pos = 0u;
    for (std::size_t i = 0; i < count; ++i) {
        const unsigned shown = static_cast<unsigned>(ids[i]) + static_cast<unsigned>(displayBias);
        const int n = (pos == 0u)
            ? std::snprintf(out + pos, outLen - pos, "%u", shown)
            : std::snprintf(out + pos, outLen - pos, ", %u", shown);
        if (n <= 0 || static_cast<std::size_t>(n) >= outLen - pos) {
            out[outLen - 1u] = '\0';
            return pos > 0u;
        }
        pos += static_cast<std::size_t>(n);
    }
    return true;
}

MConsole::BlockResult MConsole::toggleMechBlocked(uint8_t id) noexcept
{
    clearBlockMessage();

    if (!validMechId(id)) {
        return BlockResult::NoChange;
    }

    smcp::Group& blk = _groups[kBlockedGroupId];
    if (blk.mech.contains(id)) {
        blk.mech.remove(id);
        markEdited();
        return BlockResult::Changed;
    }

    /* Ручная блокировка запрещена, если лебёдка уже в заблокированной группе (кроме id 0). */
    uint8_t groupIds[smcp::kGroupMaxCount]{};
    std::size_t groupCount = 0u;
    for (std::size_t g = 1; g < smcp::kGroupMaxCount; ++g) {
        const smcp::Group& grp = _groups[g];
        if (grp.isBlocked() && grp.mech.contains(id)) {
            groupIds[groupCount++] = static_cast<uint8_t>(g);
        }
    }

    if (groupCount > 0u) {
        char list[96]{};
        (void)formatIdList(list, sizeof(list), groupIds, groupCount);
        std::snprintf(_blockMsg, sizeof(_blockMsg),
            "Cannot block manually.\nGroups: %s", list);
        return BlockResult::Rejected;
    }

    if (_mechs[id].status().any(smcp::IMech::Status::Selected)) {
        (void)_mechs[id].select(smcp::kSelectOwnerNone);
    }
    blk.mech.add(id);
    markEdited();
    return BlockResult::Changed;
}

MConsole::BlockResult MConsole::toggleGroupBlocked(uint8_t id) noexcept
{
    clearBlockMessage();

    if (!validGroupId(id) || id == kBlockedGroupId || _groups[id].isEmpty()) {
        return BlockResult::NoChange;
    }

    const bool block = !_groups[id].isBlocked();
    if (!block) {
        _groups[id].setBlocked(false);
        markEdited();
        return BlockResult::Changed;
    }

    /* Пересечение мехов только с уже заблокированными группами. */
    uint8_t mechIds[kMechCount]{};
    std::size_t mechCount = 0u;
    const smcp::Selection& self = _groups[id].mech;
    for (std::size_t m = 0; m < kMechCount; ++m) {
        if (!self.contains(static_cast<uint8_t>(m))) {
            continue;
        }
        bool shared = false;
        for (std::size_t g = 0; g < smcp::kGroupMaxCount; ++g) {
            if (g == id || !_groups[g].isBlocked() || _groups[g].isEmpty()) {
                continue;
            }
            if (_groups[g].mech.contains(static_cast<uint8_t>(m))) {
                shared = true;
                break;
            }
        }
        if (shared) {
            mechIds[mechCount++] = static_cast<uint8_t>(m);
        }
    }

    _groups[id].setBlocked(true);
    if (_activeGroup == id) {
        clearActiveGroup();
    }
    markEdited();

    if (mechCount > 0u) {
        char list[96]{};
        (void)formatIdList(list, sizeof(list), mechIds, mechCount, 1u);
        std::snprintf(_blockMsg, sizeof(_blockMsg),
            "Blocked with shared winches:\n%s", list);
        return BlockResult::Warning;
    }
    return BlockResult::Changed;
}

MConsole::BlockResult MConsole::pressMech(uint8_t id) noexcept
{
    switch (_mode) {
    case Mode::Block:
        return toggleMechBlocked(id);
    case Mode::Work:
    case Mode::Show:
    default:
        return select(id) ? BlockResult::Changed : BlockResult::NoChange;
    }
}

MConsole::BlockResult MConsole::pressGroup(uint8_t id) noexcept
{
    switch (_mode) {
    case Mode::Block:
        return toggleGroupBlocked(id);
    case Mode::Work:
    case Mode::Show:
    default:
        if (!validGroupId(id) || _groups[id].isEmpty() || _groups[id].isBlocked()) {
            return BlockResult::NoChange;
        }
        if (id == _activeGroup) {
            clearActiveGroup();
            return BlockResult::Changed;
        }
        return recallGroup(id) ? BlockResult::Changed : BlockResult::NoChange;
    }
}

void MConsole::newShow() noexcept
{
    for (smcp::Group& grp : _groups) {
        grp.clear();
    }
    initBlockedGroup();
    clearActiveGroup();
    _mode = Mode::Work;
    _showName[0] = '\0';
    clearEdited();
}

bool MConsole::loadShow(MBrowser& browser, const char* path) noexcept
{
    clearError();

    if (path == nullptr || path[0] == '\0') {
        _status = Status::NoShowOpen;
        return false;
    }

    smcp::file::SectionDesc catalog[1]{};
    smcp::file::Reader reader(browser.file(), catalog, 1u);
    if (!reader.open(path)) {
        _status = mapFileStatus(reader.status());
        return false;
    }

    const smcp::file::SectionDesc* desc = reader.find(smcp::kGroupSectionTag);
    if (desc == nullptr) {
        reader.close();
        _status = Status::MissingGrup;
        return false;
    }

    if (desc->record_count == 0 || desc->record_count > smcp::kGroupMaxCount) {
        reader.close();
        _status = Status::BadLayout;
        return false;
    }

    const std::size_t byte_size =
        static_cast<std::size_t>(desc->record_count) * sizeof(smcp::Group);
    if (static_cast<std::size_t>(desc->byte_size) != byte_size) {
        reader.close();
        _status = Status::BadLayout;
        return false;
    }

    smcp::Group loaded[smcp::kGroupMaxCount]{};
    if (!reader.readSection(smcp::kGroupSectionTag, loaded, byte_size)) {
        _status = mapFileStatus(reader.status());
        reader.close();
        return false;
    }

    reader.close();

    if (!groupsValid(loaded, desc->record_count)) {
        _status = Status::BadGroups;
        return false;
    }

    newShow();
    if (desc->record_count == smcp::kGroupMaxCount - 1u) {
        for (std::size_t i = 0; i < desc->record_count; ++i) {
            _groups[i + 1u] = loaded[i];
            _groups[i + 1u].id = static_cast<uint8_t>(i + 1u);
            _groups[i + 1u].name[smcp::kGroupNameSize - 1u] = '\0';
        }
    } else {
        for (std::size_t i = 0; i < desc->record_count; ++i) {
            _groups[i] = loaded[i];
            _groups[i].id = static_cast<uint8_t>(i);
            _groups[i].name[smcp::kGroupNameSize - 1u] = '\0';
        }
        if (!_groups[kBlockedGroupId].isBlocked()) {
            initBlockedGroup();
        }
    }

    std::strncpy(_showName, path, sizeof(_showName) - 1u);
    _showName[sizeof(_showName) - 1u] = '\0';
    _status = Status::Ok;
    return true;
}

bool MConsole::saveShow(MBrowser& browser, const char* path) noexcept
{
    clearError();

    if (path == nullptr || path[0] == '\0') {
        _status = Status::NoShowOpen;
        return false;
    }

    if (isTemplateName(showBaseName(path))) {
        _status = Status::TemplateProtected;
        return false;
    }

    /* Прямая запись в целевой путь (FatFs без LFN). */
    smcp::file::SectionDesc catalog[1]{};
    smcp::file::Writer writer(browser.file(), 1u, catalog);
    if (!writer.open(path)) {
        _status = mapFileStatus(writer.status());
        return false;
    }

    if (!writer.writeSection(smcp::kGroupSectionTag,
                             _groups,
                             sizeof(smcp::Group),
                             smcp::kGroupMaxCount)) {
        writer.close();
        _status = mapFileStatus(writer.status());
        return false;
    }

    if (!writer.finalize()) {
        _status = mapFileStatus(writer.status());
        return false;
    }

    setShowName(path);
    clearEdited();
    _status = Status::Ok;
    return true;
}

void MConsole::setShowName(const char* name) noexcept
{
    if (name == nullptr) {
        _showName[0] = '\0';
        return;
    }
    std::strncpy(_showName, name, sizeof(_showName) - 1u);
    _showName[sizeof(_showName) - 1u] = '\0';
}
