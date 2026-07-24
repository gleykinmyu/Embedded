/**
 * @file mconsole.cpp
 * @brief Реализация MConsole.
 */

#include "model/mconsole.hpp"
#include "model/mbrowser.hpp"

#include "board.hpp"
#include "core/nexDebug.hpp"
#include "UI/uiMessages.hpp"

#include <cstdio>
#include <cstring>
#include <variant>

namespace {

constexpr const char kDefaultGroupName[] = "Group";
constexpr const char kBlockedGroupName[] = "Blocked";

} // namespace


/* ========== Конструктор ========== */

MConsole::MConsole(MBrowser& browser, uint8_t console_id) noexcept
    : _browser(browser)
    , _console_id(console_id)
{
    Mech::setOwner(this);

    for (std::size_t i = 0; i < kMechCount; ++i) {
        _mechs[i] = Mech(static_cast<uint8_t>(i));
    }

    for (std::size_t i = 0; i < smcp::kGroupMaxCount; ++i) {
        _groups[i].id = static_cast<uint8_t>(i);
        _groups[i].clear();
    }

    initBlockedGroup();

    _showName[0] = '\0';
}


void MConsole::poll() noexcept
{
    smcp::msg::Packet pkt;
    while (_rx.pop(pkt)) {
        handlePacket(pkt);
    }
}


void MConsole::handlePacket(const smcp::msg::Packet& pkt) noexcept
{
    if (!pkt.hdr.isAddressedTo(_console_id)) {
        return;
    }

    if (const auto* tel = std::get_if<smcp::msg::Telemetry>(&pkt.body)) {
        handleTelemetry(pkt.hdr, *tel);
    }
}


void MConsole::handleTelemetry(const smcp::msg::Header& hdr,
                               const smcp::msg::Telemetry& body) noexcept
{
    if (!validMechId(body.mech_id)) {
        return;
    }
    _mechs[body.mech_id].onTelemetry(hdr.src_id, body);
    _telemetryDirty |= (1u << body.mech_id);
}


uint32_t MConsole::takeTelemetryDirty() noexcept
{
    const uint32_t dirty = _telemetryDirty;
    _telemetryDirty = 0;
    return dirty;
}


bool MConsole::pushSelect(smcp::msg::Select::Action action,
                          smcp::Selection selection) noexcept
{
    /* Set с пустой маской = снять всё наше; Select/Deselect без битов — noop. */
    if (selection.empty() && action != smcp::msg::Select::Action::Set) {
        return false;
    }

    smcp::msg::Select body{};
    body.action = action;
    body.selection = selection;

    smcp::msg::Packet pkt{};
    pkt.hdr.prio = smcp::msg::defaultPrio(smcp::msg::MsgId::Select);
    pkt.hdr.src_id = _console_id;
    pkt.hdr.dst_id = smcp::msg::kServerIdMin; /* mechs 0..23 → первый сегмент */
    pkt.hdr.msg_id = smcp::msg::MsgId::Select;
    pkt.hdr.pkt_id = nextPktId();
    pkt.body = body;

    if (!_tx.push(pkt)) {
        return false;
    }
    requestFlush();
    return true;
}


void MConsole::requestFlush() noexcept
{
    if (_flush != nullptr) {
        _flush();
    }
}


/* ========== Статус ========== */

const char* MConsole::statusText(Status status) noexcept
{
    switch (status) {
    case Status::Ok:
        return uiMsg::kOk;
    case Status::NoShowOpen:
        return uiMsg::kConsoleNoShowOpen;
    case Status::TemplateProtected:
        return uiMsg::kConsoleTemplateProtected;
    case Status::BadMagic:
        return uiMsg::kConsoleBadMagic;
    case Status::BadVersion:
        return uiMsg::kConsoleBadVersion;
    case Status::BadHeaderCrc:
    case Status::BadBodyCrc:
        return uiMsg::kConsoleBadCrc;
    case Status::BadLayout:
        return uiMsg::kConsoleBadLayout;
    case Status::Truncated:
        return uiMsg::kConsoleTruncated;
    case Status::MissingGrup:
        return uiMsg::kConsoleMissingGrup;
    case Status::BadGroups:
        return uiMsg::kConsoleBadGroups;
    case Status::InvalidGroup:
        return uiMsg::kConsoleInvalidGroup;
    case Status::NoSelection:
        return uiMsg::kConsoleNoSelection;
    case Status::GroupOccupied:
        return uiMsg::kConsoleGroupOccupied;
    case Status::BrowserFault:
        return uiMsg::kStorageError; /**< UI: брать текст у MBrowser. */
    case Status::InvalidName:
        return uiMsg::kBrowserInvalidName;
    case Status::IoError:
    default:
        return uiMsg::kStorageError;
    }
}


/* ========== Режим ========== */

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


MConsole::BlockResult MConsole::pressMech(uint8_t id) noexcept
{
    switch (_mode) {
    case Mode::Block:
        return toggleMechBlocked(id);
    case Mode::Work:
    case Mode::Show:
    default:
        return mechSelect(id) ? BlockResult::Changed : BlockResult::NoChange;
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


/* ========== Механизмы ========== */

bool MConsole::hasSelection() const noexcept
{
    for (const Mech& mech : _mechs) {
        if (mech.isSelected()) {
            return true;
        }
    }
    return false;
}

bool MConsole::mechSelect(uint8_t id) noexcept
{
    if (!validMechId(id) || isMechBlocked(id)) {
        return false;
    }

    Mech& mech = _mechs[id];

    if (mech.isSelected()) {
        return mech.select(smcp::kSelectOwnerNone);
    }

    if (isMechIsolated(id)) {
        return false;
    }

    return mech.select(_console_id);
}


void MConsole::clearSelection() noexcept
{
    (void)pushSelect(smcp::msg::Select::Action::Set, smcp::Selection{});
}


bool MConsole::recallGroup(uint8_t id) noexcept
{
    if (!validGroupId(id) || _groups[id].isEmpty()) {
        return false;
    }

    smcp::Selection group_sel;
    const smcp::Selection& src = _groups[id].mech;
    for (uint8_t i = 0; i < kMechCount; ++i) {
        if (src.contains(i) && !isMechBlocked(i) && !isMechIsolated(i)) {
            group_sel.add(i);
        }
    }

    (void)pushSelect(smcp::msg::Select::Action::Set, group_sel);

    _activeGroup = id;
    return true;
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


/* ========== Группы ========== */

bool MConsole::recordGroup(uint8_t id, const char* name, bool confirmed) noexcept
{
    if (!validGroupId(id) || id == kBlockedGroupId) {
        _status = Status::InvalidGroup;
        return false;
    }

    const smcp::Selection selection = selectionFromMechs();
    if (selection.empty()) {
        _status = Status::NoSelection;
        return false;
    }

    smcp::Group& grp = _groups[id];
    const bool occupied = !grp.isEmpty();
    if (occupied && !confirmed) {
        _status = Status::GroupOccupied;
        return false;
    }

    grp.id = id;
    grp.setSelection(selection);

    if (name != nullptr && name[0] != '\0') {
        grp.setName(name);
    } else if (!occupied) {
        grp.setName(kDefaultGroupName);
    }

    markEdited();
    clearActiveGroup();
    rebuildBlockedMechs();
    _status = Status::Ok;
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


bool MConsole::clearGroup(uint8_t id, bool confirmed) noexcept
{
    if (!validGroupId(id) || id == kBlockedGroupId) {
        _status = Status::InvalidGroup;
        return false;
    }

    if (_groups[id].isEmpty()) {
        _status = Status::Ok;
        return true;
    }

    if (!confirmed) {
        _status = Status::GroupOccupied;
        return false;
    }

    _groups[id].clear();
    markEdited();
    rebuildBlockedMechs();
    _status = Status::Ok;
    return true;
}


void MConsole::clearActiveGroup() noexcept
{
    clearSelection();
    _activeGroup = kBlockedGroupId;
}


void MConsole::setSettings(const Settings& settings) noexcept
{
    if (std::memcmp(&_settings, &settings, sizeof(Settings)) == 0) {
        return;
    }
    _settings = settings;
    markEdited();
}


bool MConsole::isMechIsolated(uint8_t id) const noexcept
{
    if (!_settings.isolateGroup || _activeGroup == kBlockedGroupId || !validMechId(id)) {
        return false;
    }
    return !_groups[_activeGroup].mech.contains(id);
}


void MConsole::initBlockedGroup() noexcept
{
    smcp::Group& grp = _groups[kBlockedGroupId];
    grp.clear();
    grp.id = kBlockedGroupId;
    grp.setBlocked(true);
    grp.setName(kBlockedGroupName);
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


/* ========== Блокировка ========== */

bool MConsole::isMechBlocked(uint8_t id) const noexcept
{
    return validMechId(id) && _mechs[id].isBlocked();
}

bool MConsole::fillMechBlockMessage(uint8_t id) noexcept
{
    clearBlockMessage();

    if (!isMechBlocked(id)) {
        return false;
    }

    const unsigned winchNo = static_cast<unsigned>(id) + 1u;

    uint8_t groupIds[smcp::kGroupMaxCount]{};
    const std::size_t groupCount = collectBlockingGroupIds(id, groupIds, smcp::kGroupMaxCount);

    if (groupCount > 0u) {
        char list[96]{};
        (void)formatIdList(list, sizeof(list), groupIds, groupCount);
        std::snprintf(_blockMsg, sizeof(_blockMsg),
            uiMsg::kBlockByGroupsFmt, uiMsg::kWinchMark, winchNo, list);
    } else {
        std::snprintf(_blockMsg, sizeof(_blockMsg),
            uiMsg::kBlockByManualFmt, uiMsg::kWinchMark, winchNo);
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
    const bool manual = blk.mech.contains(id);
    const unsigned winchNo = static_cast<unsigned>(id) + 1u;

    uint8_t groupIds[smcp::kGroupMaxCount]{};
    const std::size_t groupCount = collectBlockingGroupIds(id, groupIds, smcp::kGroupMaxCount);

    /* Заблокирована группами — снять лишнюю ручную (legacy), показать группы. */
    if (groupCount > 0u) {
        char list[96]{};
        (void)formatIdList(list, sizeof(list), groupIds, groupCount);
        if (manual) {
            blk.mech.remove(id);
            markEdited();
            rebuildBlockedMechs();
            std::snprintf(_blockMsg, sizeof(_blockMsg),
                uiMsg::kBlockByGroupsFmt, uiMsg::kWinchMark, winchNo, list);
            return BlockResult::Warning;
        }
        std::snprintf(_blockMsg, sizeof(_blockMsg),
            uiMsg::kBlockByGroupsFmt, uiMsg::kWinchMark, winchNo, list);
        return BlockResult::Rejected;
    }

    /* Только ручная — снять / поставить. */
    if (manual) {
        blk.mech.remove(id);
        markEdited();
        rebuildBlockedMechs();
        return BlockResult::Changed;
    }

    if (_mechs[id].isSelected()) {
        (void)_mechs[id].select(smcp::kSelectOwnerNone);
    }
    blk.mech.add(id);
    markEdited();
    rebuildBlockedMechs();
    return BlockResult::Changed;
}


MConsole::BlockResult MConsole::toggleGroupBlocked(uint8_t id) noexcept
{
    clearBlockMessage();

    if (!validGroupId(id) || id == kBlockedGroupId || _groups[id].isEmpty()) {
        return BlockResult::NoChange;
    }

    if (_groups[id].isBlocked()) {
        setGroupBlocked(id, false);
        markEdited();
        return BlockResult::Changed;
    }

    /* Пересечение с другими blocked-группами (id≥1), не с ручной (id 0). */
    uint8_t sharedIds[kMechCount]{};
    std::size_t sharedCount = 0u;
    const smcp::Selection& self = _groups[id].mech;
    for (std::size_t m = 0; m < kMechCount; ++m) {
        if (!self.contains(static_cast<uint8_t>(m))) {
            continue;
        }
        uint64_t mask = mechGroupMask(static_cast<uint8_t>(m), smcp::Group::Flag::Blocked);
        mask &= ~(1ULL << kBlockedGroupId);
        if (mask != 0ULL) {
            sharedIds[sharedCount++] = static_cast<uint8_t>(m);
        }
    }

    setGroupBlocked(id, true);
    markEdited();

    if (sharedCount > 0u) {
        char list[96]{};
        (void)formatIdList(list, sizeof(list), sharedIds, sharedCount, 1u, uiMsg::kWinchMark);
        std::snprintf(_blockMsg, sizeof(_blockMsg),
            uiMsg::kBlockSharedWinchesFmt, list);
        return BlockResult::Warning;
    }
    return BlockResult::Changed;
}

void MConsole::setGroupBlocked(uint8_t id, bool blocked) noexcept
{
    _groups[id].setBlocked(blocked);
    if (blocked) {
        smcp::Group& manual = _groups[kBlockedGroupId];
        const smcp::Selection& sel = _groups[id].mech;
        for (std::size_t m = 0; m < kMechCount; ++m) {
            if (!sel.contains(static_cast<uint8_t>(m))) {
                continue;
            }
            /* Ручная блокировка сбрасывается — источник теперь группа. */
            manual.mech.remove(static_cast<uint8_t>(m));
            if (_mechs[m].isSelected()) {
                (void)_mechs[m].select(smcp::kSelectOwnerNone);
            }
        }
        if (_activeGroup == id) {
            _activeGroup = kBlockedGroupId;
        }
    }
    rebuildBlockedMechs();
}


void MConsole::rebuildBlockedMechs() noexcept
{
    // TODO(server): show-Blocked не писать в IMech::Status; UI читает GRUP.
    // IMech::block/select — запросы; Selected/Ready/… только из Telemetry (onTelemetry).
    for (std::size_t m = 0; m < kMechCount; ++m) {
        bool blocked = false;
        for (std::size_t g = 0; g < smcp::kGroupMaxCount; ++g) {
            const smcp::Group& grp = _groups[g];
            if (grp.isBlocked() && grp.mech.contains(static_cast<uint8_t>(m))) {
                blocked = true;
                break;
            }
        }

        Mech& mech = _mechs[m];
        (void)mech.block(blocked);
    }
}


std::size_t MConsole::collectBlockingGroupIds(uint8_t mech_id,
                                             uint8_t* out,
                                             std::size_t outCap) const noexcept
{
    if (out == nullptr || outCap == 0u || !validMechId(mech_id)) {
        return 0u;
    }

    std::size_t count = 0u;
    for (std::size_t g = 1; g < smcp::kGroupMaxCount && count < outCap; ++g) {
        const smcp::Group& grp = _groups[g];
        if (grp.isBlocked() && grp.mech.contains(mech_id)) {
            out[count++] = static_cast<uint8_t>(g);
        }
    }
    return count;
}


void MConsole::clearBlockMessage() noexcept
{
    _blockMsg[0] = '\0';
}


bool MConsole::formatIdList(char* out,
                            std::size_t outLen,
                            const uint8_t* ids,
                            std::size_t count,
                            uint8_t displayBias,
                            const char* mark) noexcept
{
    if (out == nullptr || outLen == 0u) {
        return false;
    }
    out[0] = '\0';
    if (ids == nullptr || count == 0u) {
        return false;
    }

    const char* const prefix = (mark != nullptr) ? mark : "";

    std::size_t pos = 0u;
    for (std::size_t i = 0; i < count; ++i) {
        const unsigned shown = static_cast<unsigned>(ids[i]) + static_cast<unsigned>(displayBias);
        const int n = (pos == 0u)
            ? std::snprintf(out + pos, outLen - pos, "%s%u", prefix, shown)
            : std::snprintf(out + pos, outLen - pos, ", %s%u", prefix, shown);
        if (n <= 0 || static_cast<std::size_t>(n) >= outLen - pos) {
            out[outLen - 1u] = '\0';
            return pos > 0u;
        }
        pos += static_cast<std::size_t>(n);
    }
    return true;
}


/* ========== Шоуфайл ========== */

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


void MConsole::setShowName(const char* name) noexcept
{
    if (name == nullptr) {
        _showName[0] = '\0';
        return;
    }
    std::strncpy(_showName, name, sizeof(_showName) - 1u);
    _showName[sizeof(_showName) - 1u] = '\0';
}


void MConsole::newShow() noexcept
{
    for (smcp::Group& grp : _groups) {
        grp.clear();
    }
    initBlockedGroup();
    clearActiveGroup();
    _settings = Settings{};
    _mode = Mode::Work;
    _showName[0] = '\0';
    clearEdited();
    rebuildBlockedMechs();
}


bool MConsole::openShow() noexcept
{
    char path[smcp::file::kPathSize]{};
    if (!_browser.prepareOpenSelected(path, sizeof(path))) {
        _status = Status::BrowserFault;
        return false;
    }
    if (path[0] == '\0') {
        _status = Status::NoShowOpen;
        return false;
    }
    if (!importShow(_browser.file(), path)) {
        return false;
    }
    persistMirror();
    return true;
}


bool MConsole::saveShow() noexcept
{
    return saveShowPath(_showName);
}


bool MConsole::saveShowAs(const char* name, bool confirmed) noexcept
{
    if (name != nullptr && name[0] != '\0') {
        std::strncpy(_saveAsName, name, sizeof(_saveAsName) - 1u);
        _saveAsName[sizeof(_saveAsName) - 1u] = '\0';
    }

    if (_saveAsName[0] == '\0') {
        _status = Status::InvalidName;
        return false;
    }

    if (!canMutateShowName(_saveAsName)) {
        return false;
    }

    char path[smcp::file::kPathSize]{};
    if (!_browser.prepareSaveAs(_saveAsName, path, sizeof(path))) {
        _status = Status::BrowserFault;
        if (_browser.getStatus() != MBrowser::Status::FileExists || !confirmed) {
            return false;
        }
    }

    return saveShowPath(path);
}


bool MConsole::saveShowPath(const char* path) noexcept
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

    if (!_browser.ensureReady()) {
        _status = Status::BrowserFault;
        return false;
    }

    /* --- Атомарная подмена через tmp на томе --- */
    static constexpr char kTempBaseName[] = "tmp";
    char tempPath[smcp::file::kPathSize]{};
    if (!_browser.makePath(tempPath, sizeof(tempPath), kTempBaseName)) {
        _status = Status::BrowserFault;
        return false;
    }

    BIF::IVolume& volume = _browser.volume();
    (void)volume.remove(tempPath);

    /* Имя в заголовке — финальный путь (openPath при записи = tmp). */
    char prevName[smcp::file::kPathSize]{};
    std::strncpy(prevName, _showName, sizeof(prevName) - 1u);
    setShowName(path);

    if (!exportShow(_browser.file(), tempPath)) {
        setShowName(prevName);
        (void)volume.remove(tempPath);
        return false;
    }

    if (std::strcmp(tempPath, path) != 0) {
        if (volume.exists(path) && !volume.remove(path)) {
            setShowName(prevName);
            (void)volume.remove(tempPath);
            _status = Status::IoError;
            return false;
        }
        if (!volume.rename(tempPath, path)) {
            setShowName(prevName);
            (void)volume.remove(tempPath);
            _status = Status::IoError;
            return false;
        }
    }

    clearEdited();
    _status = Status::Ok;
    persistMirror();
    return true;
}


bool MConsole::importShow(BIF::IFile& io, const char* openPath) noexcept
{
    clearError();

    if (openPath == nullptr) {
        openPath = "";
    }

    /* --- Открыть шоуфайл: SETT (пульт, опц.) + GRUP --- */
    smcp::file::SectionDesc catalog[2]{};
    smcp::file::Reader reader(io, catalog, 2u);
    if (!reader.open(openPath)) {
        _status = mapFileStatus(reader.status());
        return false;
    }

    Settings loadedSettings{};
    const smcp::file::SectionDesc* sett = reader.find(kSettingsSectionTag);
    if (sett != nullptr) {
        if (sett->record_count != 1u
            || static_cast<std::size_t>(sett->byte_size) != kSettingsWireSize) {
            reader.close();
            _status = Status::BadLayout;
            return false;
        }
        if (!reader.readSection(kSettingsSectionTag, &loadedSettings, sizeof(loadedSettings))) {
            _status = mapFileStatus(reader.status());
            reader.close();
            return false;
        }
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

    /* --- _groupsTmp: читать → groupsValid → commit в _groups (BadGroups не трогает текущее шоу). --- */
    if (!reader.readSection(smcp::kGroupSectionTag, _groupsTmp, byte_size)) {
        _status = mapFileStatus(reader.status());
        reader.close();
        return false;
    }

    char nameFromHdr[smcp::file::kPathSize]{};
    std::strncpy(nameFromHdr, reader.header().name, sizeof(nameFromHdr) - 1u);
    reader.close();

    if (!groupsValid(_groupsTmp, desc->record_count)) {
        _status = Status::BadGroups;
        return false;
    }

    /* --- Commit: сброс рабочего шоу, затем копия из staging ---
       count == Max-1 → файл без Blocked (слоты 1..N); иначе полный массив. */
    for (smcp::Group& grp : _groups) {
        grp.clear();
    }
    initBlockedGroup();
    clearActiveGroup();
    _settings = loadedSettings;
    _mode = Mode::Work;

    if (desc->record_count == smcp::kGroupMaxCount - 1u) {
        for (std::size_t i = 0; i < desc->record_count; ++i) {
            _groups[i + 1u] = _groupsTmp[i];
            _groups[i + 1u].id = static_cast<uint8_t>(i + 1u);
            _groups[i + 1u].name[smcp::kGroupNameSize - 1u] = '\0';
        }
    } else {
        for (std::size_t i = 0; i < desc->record_count; ++i) {
            _groups[i] = _groupsTmp[i];
            _groups[i].id = static_cast<uint8_t>(i);
            _groups[i].name[smcp::kGroupNameSize - 1u] = '\0';
        }
        if (!_groups[kBlockedGroupId].isBlocked()) {
            initBlockedGroup();
        }
    }

    setShowName(openPath[0] != '\0' ? openPath : nameFromHdr);
    clearEdited();
    rebuildBlockedMechs();
    _status = Status::Ok;
    return true;
}


bool MConsole::exportShow(BIF::IFile& io, const char* openPath) noexcept
{
    clearError();

    if (openPath == nullptr) {
        openPath = "";
    }

    smcp::file::SectionDesc catalog[2]{};
    smcp::file::Writer writer(io, 2u, catalog);
    if (!writer.open(openPath)) {
        _status = mapFileStatus(writer.status());
        return false;
    }
    if (!writer.writeSection(kSettingsSectionTag,
                             &_settings,
                             sizeof(Settings),
                             1u)) {
        writer.close();
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

    writer.setName(_showName);

    if (!writer.finalize()) {
        _status = mapFileStatus(writer.status());
        return false;
    }

    _status = Status::Ok;
    return true;
}


void MConsole::persistMirror() noexcept
{
    if (_mirror == nullptr) {
        return;
    }
    /* SD уже сохранён — не затирать Ok ошибкой зеркала. */
    const Status saved = _status;
    board.watchdog.kick();
    if (!exportShow(*_mirror, "")) {
        NEX_DBG("persistMirror failed: %s (name='%s')\n", statusText(_status), _showName);
        _status = saved;
        return;
    }
    _status = saved;
    NEX_DBG("persistMirror OK: '%s'\n", _showName);
}


bool MConsole::restoreMirror() noexcept
{
    if (_mirror == nullptr) {
        return false;
    }
    board.watchdog.kick();
    /* Пустой openPath → имя из Header::name (хвост сектора не используется). */
    if (!importShow(*_mirror, "")) {
        NEX_DBG("restoreMirror failed: %s\n", statusText(_status));
        return false;
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
