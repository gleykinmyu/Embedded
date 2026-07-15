/**
 * @file mconsole.cpp
 * @brief Реализация MConsole.
 */

#include "mconsole.hpp"

#include <cstring>

namespace {

constexpr const char kDefaultGroupName[] = "Group";
constexpr const char kBlockedGroupName[] = "Blocked";

} // namespace

void MConsole::initBlockedGroup() noexcept
{
    smcp::Group& grp = _groups[kBlockedGroupId];
    grp.clear();
    grp.id = kBlockedGroupId;
    grp.setBlocked(true);
    grp.setName(kBlockedGroupName);
}

MConsole::MConsole(smcp::file::IFile& io, uint8_t max_active_mechs, uint8_t console_id) noexcept
    : _io(io)
    , _console_id(console_id)
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
        return true;
    }

    if (name != nullptr && name[0] != '\0') {
        grp.setName(name);
    } else if (!overwrite) {
        grp.setName(kDefaultGroupName);
    }

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
    return true;
}

bool MConsole::clearGroup(uint8_t id) noexcept
{
    if (!validGroupId(id)) {
        return false;
    }

    if (id == kBlockedGroupId) {
        _groups[id].mech = smcp::Selection{};
        return true;
    }

    _groups[id].clear();
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

void MConsole::newShow() noexcept
{
    for (smcp::Group& grp : _groups) {
        grp.clear();
    }
    initBlockedGroup();
    clearActiveGroup();
    _showName[0] = '\0';
}

bool MConsole::loadShow(const char* path) noexcept
{
    if (path == nullptr) {
        return false;
    }

    smcp::file::SectionDesc catalog[1]{};
    smcp::file::Reader reader(_io, catalog, 1u);
    if (!reader.open(path)) {
        return false;
    }

    const smcp::file::SectionDesc* desc = reader.find(smcp::kGroupSectionTag);
    if (desc == nullptr) {
        reader.close();
        return false;
    }

    if (desc->record_count == 0 || desc->record_count > smcp::kGroupMaxCount) {
        reader.close();
        return false;
    }

    smcp::Group loaded[smcp::kGroupMaxCount]{};
    const std::size_t byte_size =
        static_cast<std::size_t>(desc->record_count) * sizeof(smcp::Group);
    if (!reader.readSection(smcp::kGroupSectionTag, loaded, byte_size)) {
        reader.close();
        return false;
    }

    reader.close();

    newShow();
    if (desc->record_count == smcp::kGroupMaxCount - 1u) {
        for (std::size_t i = 0; i < desc->record_count; ++i) {
            _groups[i + 1u] = loaded[i];
            _groups[i + 1u].id = static_cast<uint8_t>(i + 1u);
        }
    } else {
        for (std::size_t i = 0; i < desc->record_count; ++i) {
            _groups[i] = loaded[i];
            _groups[i].id = static_cast<uint8_t>(i);
        }
        if (!_groups[kBlockedGroupId].isBlocked()) {
            initBlockedGroup();
        }
    }

    std::strncpy(_showName, path, sizeof(_showName) - 1u);
    _showName[sizeof(_showName) - 1u] = '\0';
    return true;
}

bool MConsole::saveShow(const char* path) noexcept
{
    if (path == nullptr) {
        return false;
    }

    smcp::file::SectionDesc catalog[1]{};
    smcp::file::Writer writer(_io, 1u, catalog);
    if (!writer.open(path)) {
        return false;
    }

    if (!writer.writeSection(smcp::kGroupSectionTag,
                             _groups,
                             sizeof(smcp::Group),
                             smcp::kGroupMaxCount)) {
        writer.close();
        return false;
    }

    if (!writer.finalize()) {
        return false;
    }

    setShowName(path);
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
