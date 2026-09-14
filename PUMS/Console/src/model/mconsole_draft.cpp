/**
 * @file mconsole_draft.cpp
 * @brief Черновик нового MConsole: композиция, import через второй Show.
 */

#include "model/mconsole_draft.hpp"

#include "UI/uiMessages.hpp"

#include <cstring>

namespace draft {
namespace {

constexpr const char kTempBaseName[] = "tmp";

} // namespace

MConsole::MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, smcp::ILink& link,
                   smcp::Node::ClockFn clock) noexcept
    : Console(link, clock)
    , cmechs(*this)
    , show(*this)
    , browser(volume, dir)
    , _scratch(*this)
    , _file(file)
{}

const char* MConsole::statusText(Status status) noexcept
{
    switch (status) {
    case Status::Ok: return uiMsg::kOk;
    case Status::NoShowOpen: return uiMsg::kConsoleNoShowOpen;
    case Status::TemplateProtected: return uiMsg::kConsoleTemplateProtected;
    case Status::BadMagic: return uiMsg::kConsoleBadMagic;
    case Status::BadVersion: return uiMsg::kConsoleBadVersion;
    case Status::BadHeaderCrc:
    case Status::BadBodyCrc: return uiMsg::kConsoleBadCrc;
    case Status::BadLayout: return uiMsg::kConsoleBadLayout;
    case Status::Truncated: return uiMsg::kConsoleTruncated;
    case Status::MissingGrup: return uiMsg::kConsoleMissingGrup;
    case Status::BadGroups: return uiMsg::kConsoleBadGroups;
    case Status::InvalidName: return uiMsg::kBrowserInvalidName;
    case Status::NotFound: return uiMsg::kBrowserNotFound;
    case Status::FileExists: return uiMsg::kBrowserFileExists;
    case Status::NotMounted: return uiMsg::kBrowserNotMounted;
    case Status::OpenDirFailed: return uiMsg::kBrowserOpenDirFailed;
    case Status::PathTooLong: return uiMsg::kBrowserPathTooLong;
    case Status::OpenFileProtected: return uiMsg::kBrowserOpenProtected;
    case Status::IoError:
    default: return uiMsg::kStorageError;
    }
}

MConsole::Status MConsole::mapFile(smcp::file::Status st) noexcept
{
    switch (st) {
    case smcp::file::Status::Ok: return Status::Ok;
    case smcp::file::Status::BadMagic: return Status::BadMagic;
    case smcp::file::Status::BadVersion: return Status::BadVersion;
    case smcp::file::Status::BadHeaderCrc: return Status::BadHeaderCrc;
    case smcp::file::Status::BadBodyCrc: return Status::BadBodyCrc;
    case smcp::file::Status::BadLayout: return Status::BadLayout;
    case smcp::file::Status::Truncated: return Status::Truncated;
    case smcp::file::Status::IoError:
    default: return Status::IoError;
    }
}

MConsole::Status MConsole::mapBrowser(smcp::file::IBrowser::Status st) noexcept
{
    switch (st) {
    case smcp::file::IBrowser::Status::Ok: return Status::Ok;
    case smcp::file::IBrowser::Status::NotMounted: return Status::NotMounted;
    case smcp::file::IBrowser::Status::OpenDirFailed: return Status::OpenDirFailed;
    case smcp::file::IBrowser::Status::InvalidName: return Status::InvalidName;
    case smcp::file::IBrowser::Status::PathTooLong: return Status::PathTooLong;
    case smcp::file::IBrowser::Status::NotFound: return Status::NotFound;
    case smcp::file::IBrowser::Status::FileExists: return Status::FileExists;
    case smcp::file::IBrowser::Status::IoError:
    default: return Status::IoError;
    }
}

bool MConsole::ensureDir() noexcept
{
    if (browser.dirPath()[0] == '\0' && !browser.open(nullptr)) {
        return fail(mapBrowser(browser.status()));
    }
    return true;
}

bool MConsole::hasSelection() const noexcept
{
    for (uint8_t i = 0; i < kMechCount; ++i) {
        if (cmechs[i].isSelectedBy(Console::id())) {
            return true;
        }
    }
    return false;
}

bool MConsole::groupsValid(const Show& show) noexcept
{
    constexpr uint8_t kKnown = static_cast<uint8_t>(smcp::Group::Flag::Blocked)
        | static_cast<uint8_t>(smcp::Group::Flag::Atomic);
    for (uint8_t i = 0; i < smcp::kGroupMaxCount; ++i) {
        const smcp::CGroup g = show.grup[i];
        if ((g.flags().raw() & static_cast<uint8_t>(~kKnown)) != 0u) {
            return false;
        }
        if (std::memchr(g.name(), '\0', smcp::kGroupNameSize) == nullptr) {
            return false;
        }
    }
    return true;
}

bool MConsole::commitScratch() noexcept
{
    if (!_scratch.allRequiredPresent()) {
        return fail(Status::MissingGrup);
    }
    if (!groupsValid(_scratch)) {
        return fail(Status::BadGroups);
    }
    for (uint8_t i = 0; i < kGroupCount; ++i) {
        smcp::Group rec{};
        _scratch.grup[i].writeTo(rec);
        show.grup[i].readFrom(rec);
    }
    *show.sett.begin() = *_scratch.sett.begin();
    return ok();
}

bool MConsole::importFrom(BIF::IFile& io, const char* path) noexcept
{
    const smcp::file::Status st = _scratch.load(io, path);
    if (st != smcp::file::Status::Ok) {
        return fail(mapFile(st));
    }
    if (!commitScratch()) {
        return false;
    }
    show.setName((path != nullptr && path[0] != '\0') ? path : _scratch.name());
    show.clearEdited();
    notifyShow();
    return true;
}

bool MConsole::exportTo(BIF::IFile& io, const char* path) noexcept
{
    const smcp::file::Status st = show.save(io, path);
    if (st != smcp::file::Status::Ok) {
        return fail(mapFile(st));
    }
    return ok();
}

void MConsole::newShow() noexcept
{
    for (uint8_t i = 0; i < kGroupCount; ++i) {
        (void)show.grup[i].clear(true);
    }
    *show.sett.begin() = Settings{};
    show.setName(nullptr);
    _activeGroup = kNoActiveGroup;
    _pendingActiveGroup = kNoActiveGroup;
    _groupSelectPending = true;
    Console::clearSelection();
    _mode = Mode::Work;
    show.clearEdited();
    notifyShow();
}

bool MConsole::openShow(const char* name) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[smcp::file::kPathSize]{};
    if (!browser.makePath(path, sizeof(path), name)) {
        return fail(Status::InvalidName);
    }
    if (!importFrom(_file, path)) {
        return false;
    }
    persistMirror();
    return true;
}

bool MConsole::saveShow() noexcept
{
    if (show.name()[0] == '\0') {
        return fail(Status::NoShowOpen);
    }
    return saveShowAs(showBaseName(show.name()), true);
}

bool MConsole::saveShowAs(const char* name, bool confirmed) noexcept
{
    if (name == nullptr || name[0] == '\0') {
        return fail(Status::InvalidName);
    }
    if (isTemplateName(showBaseName(name))) {
        return fail(Status::TemplateProtected);
    }
    if (!ensureDir()) {
        return false;
    }
    char path[smcp::file::kPathSize]{};
    if (!browser.makePath(path, sizeof(path), name)) {
        return fail(Status::InvalidName);
    }
    char tmp[smcp::file::kPathSize]{};
    if (!browser.makePath(tmp, sizeof(tmp), kTempBaseName)) {
        return fail(Status::PathTooLong);
    }

    if (!browser.refresh()) {
        return fail(mapBrowser(browser.status()));
    }
    bool exists = false;
    for (uint16_t i = 0; i < browser.cacheCount(); ++i) {
        if (std::strcmp(browser[i].name, name) == 0) {
            exists = true;
            break;
        }
    }
    if (exists && !confirmed) {
        return fail(Status::FileExists);
    }

    show.setName(path);
    if (!exportTo(_file, tmp)) {
        (void)browser.remove(kTempBaseName);
        return false;
    }
    if (std::strcmp(kTempBaseName, name) != 0) {
        if (exists && !browser.remove(name)) {
            (void)browser.remove(kTempBaseName);
            return fail(mapBrowser(browser.status()));
        }
        if (!browser.rename(kTempBaseName, name)) {
            (void)browser.remove(kTempBaseName);
            return fail(mapBrowser(browser.status()));
        }
    }
    show.clearEdited();
    persistMirror();
    notifyShow();
    return ok();
}

bool MConsole::removeShow(const char* name) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[smcp::file::kPathSize]{};
    if (!browser.makePath(path, sizeof(path), name)) {
        return fail(Status::InvalidName);
    }
    if (show.name()[0] != '\0' && std::strcmp(path, show.name()) == 0) {
        return fail(Status::OpenFileProtected);
    }
    if (!browser.remove(name)) {
        return fail(mapBrowser(browser.status()));
    }
    return ok();
}

bool MConsole::restoreMirror() noexcept
{
    if (_mirror == nullptr) {
        return false;
    }
    return importFrom(*_mirror, "");
}

void MConsole::persistMirror() noexcept
{
    if (_mirror == nullptr) {
        return;
    }
    const Status saved = _status;
    (void)exportTo(*_mirror, "");
    _status = saved;
}

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

void MConsole::notifyShow() noexcept
{
    if (_onShowChanged != nullptr) {
        _onShowChanged();
    }
}

void MConsole::onTelemetry(const smcp::msg::Header& hdr, const smcp::msg::Telemetry& body) noexcept
{
    Console::onTelemetry(hdr, body);
    if (_onMechChanged != nullptr) {
        _onMechChanged(body.mech_id);
    }
}

void MConsole::onAck(smcp::Session* session, const smcp::TxSlot& req) noexcept
{
    Node::onAck(session, req);
    if (session != &_primary) {
        return;
    }
    if (smcp::msg::helpers::msgIdOf(req.body) != smcp::msg::MsgId::Select) {
        return;
    }
    if (_groupSelectPending) {
        _activeGroup = _pendingActiveGroup;
        _groupSelectPending = false;
    }
    if (_onSelectAck != nullptr) {
        _onSelectAck();
    }
}

void MConsole::onNack(smcp::Session* session, const smcp::TxSlot& req,
                      const smcp::msg::Nack& reply) noexcept
{
    Node::onNack(session, req, reply);
    if (session != &_primary) {
        return;
    }
    _lastNackReq = smcp::msg::helpers::msgIdOf(req.body);
    _lastNack = reply.code;
    _lastNackDetail = reply.detail;
    if (_lastNackReq == smcp::msg::MsgId::Select && _groupSelectPending) {
        _groupSelectPending = false;
    }
    if (_onNack != nullptr) {
        _onNack();
    }
}

void MConsole::onPhase(Phase phase) noexcept
{
    if (phase == Phase::Online) {
        smcp::Selection mask;
        for (uint8_t i = 0; i < kMechCount; ++i) {
            mask.add(i);
        }
        Console::getTelemetry(mask);
    }
    if (_onPhase != nullptr) {
        _onPhase(phase);
    }
}

} // namespace draft
