/**
 * @file mconsole_draft.cpp
 * @brief Черновик нового MConsole: ShowStore + группы / механизмы.
 */

#include "model/mconsole_draft.hpp"

#include "UI/uiMessages.hpp"

namespace draft {

MConsole::MConsole(BIF::IVolume& volume, BIF::IDirectory& dir, BIF::IFile& file, BIF::IFile& bak,
                   smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
    : Console(link, clock)
    , cmechs(*this)
    , _incoming(*this)
    , _live(*this)
    , _browser(volume, dir)
    , _store(_browser, _live, _incoming, file, bak)
{}

const char* MConsole::statusText() const noexcept
{
    switch (_status) {
    case Status::Ok: return uiMsg::kOk;
    case Status::NoShowOpen: return uiMsg::kConsoleNoShowOpen;
    case Status::MissingGrup: return uiMsg::kConsoleMissingGrup;
    case Status::BadGroups: return uiMsg::kConsoleBadGroups;
    case Status::OpenFileProtected: return uiMsg::kBrowserOpenProtected;
    case Status::BrowserFail:
        switch (_browser.status()) {
        case smcp::file::IBrowser::Status::NotMounted: return uiMsg::kBrowserNotMounted;
        case smcp::file::IBrowser::Status::OpenDirFailed: return uiMsg::kBrowserOpenDirFailed;
        case smcp::file::IBrowser::Status::InvalidName: return uiMsg::kBrowserInvalidName;
        case smcp::file::IBrowser::Status::NotFound: return uiMsg::kBrowserNotFound;
        case smcp::file::IBrowser::Status::FileExists: return uiMsg::kBrowserFileExists;
        case smcp::file::IBrowser::Status::PathTooLong: return uiMsg::kBrowserPathTooLong;
        case smcp::file::IBrowser::Status::IoError:
        default: return uiMsg::kStorageError;
        }
    case Status::MainFail:
    case Status::BakFail:
    case Status::RestoreFail:
        switch (_incoming.status() != smcp::file::Status::Ok ? _incoming.status()
                                                            : _live.status()) {
        case smcp::file::Status::BadMagic: return uiMsg::kConsoleBadMagic;
        case smcp::file::Status::BadVersion: return uiMsg::kConsoleBadVersion;
        case smcp::file::Status::BadHeaderCrc:
        case smcp::file::Status::BadBodyCrc: return uiMsg::kConsoleBadCrc;
        case smcp::file::Status::BadLayout: return uiMsg::kConsoleBadLayout;
        case smcp::file::Status::Truncated: return uiMsg::kConsoleTruncated;
        case smcp::file::Status::IoError:
        default: return uiMsg::kStorageError;
        }
    default: return uiMsg::kStorageError;
    }
}

MConsole::Status MConsole::mapStore(smcp::file::ShowStore::Status st) noexcept
{
    switch (st) {
    case smcp::file::ShowStore::Status::Ok: return Status::Ok;
    case smcp::file::ShowStore::Status::NoShowOpen: return Status::NoShowOpen;
    case smcp::file::ShowStore::Status::MissingSection: return Status::MissingGrup;
    case smcp::file::ShowStore::Status::InvalidData: return Status::BadGroups;
    case smcp::file::ShowStore::Status::OpenFileProtected: return Status::OpenFileProtected;
    case smcp::file::ShowStore::Status::BrowserFail: return Status::BrowserFail;
    case smcp::file::ShowStore::Status::MainFail: return Status::MainFail;
    case smcp::file::ShowStore::Status::BakFail: return Status::BakFail;
    case smcp::file::ShowStore::Status::RestoreFail:
    default: return Status::RestoreFail;
    }
}

bool MConsole::failStore() noexcept
{
    if (_store.status() != smcp::file::ShowStore::Status::Ok) {
        return fail(mapStore(_store.status()));
    }
    _status = Status::Ok;
    return false;
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

void MConsole::newShow() noexcept
{
    _store.newShow();
    _activeGroup = kNoActiveGroup;
    _pendingActiveGroup = kNoActiveGroup;
    _groupSelectPending = true;
    Console::clearSelection();
    _mode = Mode::Work;
    notifyShow();
}

bool MConsole::openShow(const char* name) noexcept
{
    if (!_store.openShow(name)) {
        return failStore();
    }
    notifyShow();
    return ok();
}

bool MConsole::saveShow() noexcept
{
    if (!_store.saveShow()) {
        return failStore();
    }
    notifyShow();
    return ok();
}

bool MConsole::saveShowAs(const char* name, bool confirmed) noexcept
{
    if (!_store.saveShowAs(name, confirmed)) {
        return failStore();
    }
    notifyShow();
    return ok();
}

bool MConsole::removeShow(const char* name) noexcept
{
    if (!_store.removeShow(name)) {
        return failStore();
    }
    return ok();
}

bool MConsole::restoreMirror() noexcept
{
    if (!_store.restore()) {
        return failStore();
    }
    notifyShow();
    return ok();
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
