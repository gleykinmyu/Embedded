#include "nexApplication.hpp"
#include "../core/nexTimeout.hpp"
#include "../core/nexStatusMask.hpp"

#include <variant>

#include "../core/nexDebug.hpp"

namespace nex {

Application::Application(BIF::IByteStream& stream, Rect screen, ClockMsFn clockMs,
    uint32_t timeout_ms) noexcept
    : ep(*this)
    , fs(*this)
    , cs(*this)
    , audio(*this)
    , touch(*this)
    , sleep(*this)
    , msgBox(*this)
    , bkcmd(*this, "bkcmd", SysVarTag::Bkcmd, BkCmd::OnFailure)
    , sys{SysVar<uint32_t>(*this, "sys0", SysVarTag::Sys0), SysVar<uint32_t>(*this, "sys1", SysVarTag::Sys1),
        SysVar<uint32_t>(*this, "sys2", SysVarTag::Sys2)}
    , pio{SysVar<uint8_t>(*this, "pio0", SysVarTag::Pio0), SysVar<uint8_t>(*this, "pio1", SysVarTag::Pio1),
        SysVar<uint8_t>(*this, "pio2", SysVarTag::Pio2), SysVar<uint8_t>(*this, "pio3", SysVarTag::Pio3),
        SysVar<uint8_t>(*this, "pio4", SysVarTag::Pio4), SysVar<uint8_t>(*this, "pio5", SysVarTag::Pio5),
        SysVar<uint8_t>(*this, "pio6", SysVarTag::Pio6), SysVar<uint8_t>(*this, "pio7", SysVarTag::Pio7)}
    , _screen(screen.w, screen.h)
    , _stream(stream)
    , _gateway(stream)
    , _clockMsFn(clockMs)
    , _timeoutMs(timeout_ms)
{
    NEX_ASSERT(clockMs != nullptr);
#if defined(NEX_LOG_TICKS)
    detail::setNexLogTickFn(clockMs);
#endif
}

void Application::onTimeout(const Transaction& active) noexcept {
    dispatchError(appErrorFrom(Session::Status::Timeout), active.page_id, active.comp_id);
}

void Application::abortSessionFault() noexcept {
    if (const Transaction* const tx = _session.current())
        dispatchError(appErrorFrom(_session.getStatus(), _gateway.getStatus()), tx->page_id, tx->comp_id);
}

void nexComponentRegisterFailed(Application& app, Page& page, const Component& c, MISC::RegStatus st) noexcept {
    app.dispatchError(appErrorFrom(st), page.ID, c.id());
}

Page* Application::getPage(uint8_t id) noexcept {
    return _pageStorage.get(id);
}

Component* Application::getComponent(uint8_t page_id, uint8_t comp_id) noexcept {
    Page* const p = getPage(page_id);
    if (p == nullptr)
        return nullptr;
    return p->getComponent(comp_id);
}

void Application::registerPage(Page& page) noexcept {
    (void)_pageStorage.registerSeq(&page, page.ID);
}

bool Application::tryEnqueue(Transaction tx) noexcept {
    if (_session.tryEnqueue(tx))
        return true;

    const Session::Status st = _session.getStatus();
    if (st != Session::Status::QueueFull)
        dispatchError(appErrorFrom(st), tx.page_id, tx.comp_id);
    return false;
}

void Application::enqueue(Transaction tx) noexcept {
    MsTimer stall;
    stall.start(nowMs(), _timeoutMs);

    for (;;) {
        if (_session.tryEnqueue(tx))
            return;

        const Session::Status st = _session.getStatus();
        if (st != Session::Status::QueueFull) {
            dispatchError(appErrorFrom(st), tx.page_id, tx.comp_id);
            return;
        }

        const std::size_t queued_before = _session.queuedCount();
        update();
        if (_session.queuedCount() < queued_before || _session.isActive())
            stall.start(nowMs(), _timeoutMs);

        if (stall.timedOut(nowMs()))
            break;
    }

    dispatchError(appErrorFrom(Session::Status::QueueFull), tx.page_id, tx.comp_id);
}

bool Application::pumpUntilIdle() noexcept {
    MsTimer stall;
    stall.start(nowMs(), _timeoutMs);

    while (_session.isActive() || _session.hasQueued()) {
        const std::size_t queued_before = _session.queuedCount();
        update();
        if (_session.queuedCount() < queued_before || _session.isActive())
            stall.start(nowMs(), _timeoutMs);
        if (stall.timedOut(nowMs()))
            break;
    }

    return !_session.isActive() && !_session.hasQueued();
}

void Application::update() noexcept {
    const uint32_t now_ms = nowMs();

    if (!_session.begin(_gateway) && _session.getStatus() == Session::Status::PushFailed) {
        abortSessionFault();
        _session.end(false);
    }

    if (!_session.transmit(_gateway)) {
        abortSessionFault();
        _session.end(false);
    }

    if (_session.pollTimeout(_gateway, now_ms, _timeoutMs, bkcmd)) {
        const Session::Status st = _session.getStatus();
        if (st == Session::Status::Timeout) {
            if (const Transaction* tx = _session.current())
                onTimeout(*tx);
            _session.end(false);
        } else if (st == Session::Status::Complete)
            _session.end(true);
    }

    Message m{};
    while (_gateway.receive(m)) {
        if (dispatchResponse(m, _gateway.isTxIdle()))
            continue;
        dispatchEvent(m);
    }

    const Gateway::Status gwSt = _gateway.getStatus();
    const BIF::IByteStream::Status streamSt = _stream.getStatus();
    if (gwSt != Gateway::Status::OK || streamSt != BIF::IByteStream::Status::OK)
        dispatchError(appErrorFrom(gwSt, streamSt), 0u, 0u);
    else if (_lastError.status != msg::Status::Code::Success)
        _lastError = msg::Status{};
}

void Application::dispatchError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept {
    if (_lastError.status != msg::Status::Code::Success && status == _lastError
        && page_id == _lastErrorPage && comp_id == _lastErrorComp)
        return;

    _lastError = status;
    _lastErrorPage = page_id;
    _lastErrorComp = comp_id;
    onError(status, page_id, comp_id);
    if (page_id == 0u && comp_id == 0u)
        return;
    if (Page* const p = getPage(page_id))
        p->onError(status, comp_id);
}

bool Application::dispatchResponse(const Message& m, bool txIdle) noexcept {
    if (!txIdle || !_session.isActive())
        return false;

    const Transaction* const active = _session.current();
    if (active == nullptr)
        return false;

    if (const auto* st = std::get_if<msg::Status>(&m)) {
        if (!active->correlatesWith(*st, bkcmd)) {
            dispatchError(*st, 0u, 0u);
            return true;
        }
        dispatchError(*st, active->page_id, active->comp_id);
        const bool ok = (active->kind == Transaction::Kind::Command) && st->isOK();
        _session.end(ok);
        return true;
    }

    switch (active->kind) {
    case Transaction::Kind::GetNumeric:
        if (const auto* nr = std::get_if<msg::getNumeric>(&m)) {
            if (Route::isSysVar(active->page_id, active->comp_id)) {
                dispatchSysVarResponse(active->tag, *nr);
                onSysResponse(active->tag, *nr);
            } else if (Component* const c = getComponent(active->page_id, active->comp_id))
                c->onResponse(active->tag, *nr);
            _session.end(true);
            return true;
        }
        return false;

    case Transaction::Kind::GetString:
        if (const auto* sr = std::get_if<msg::getString>(&m)) {
            if (Component* const c = getComponent(active->page_id, active->comp_id))
                c->onResponse(active->tag, *sr);
            _session.end(true);
            return true;
        }
        return false;

    case Transaction::Kind::Command:
        return false;

    case Transaction::Kind::TransparentTx:
        // NEX-R301: `evTransparent` + follow-up bytes; `pollTimeout` для transparent mode.
        if (const auto* tr = std::get_if<msg::evTransparent>(&m)) {
            (void)tr;
            // onTransparentEvent(*tr); _session.end(...);
            return false;
        }
        return false;

    case Transaction::Kind::RawDataRx:
        // NEX-R301: raw RX chunks после преамбулы read/raw.
        return false;
    }
}

void Application::dispatchEvent(const Message& m) noexcept {
    if (const auto* e = std::get_if<msg::evPage>(&m)) {
        onPageChange(e->page_id);
        return;
    }
    if (const auto* e = std::get_if<msg::Status>(&m)) {
        dispatchError(*e, 0u, 0u);
        return;
    }
    if (const auto* e = std::get_if<msg::evTouch>(&m)) {
        onTouch(*e);
        return;
    }
    if (const auto* e = std::get_if<msg::evTouchXY>(&m)) {
        onTouchXY(*e);
        return;
    }
    if (const auto* e = std::get_if<msg::evSystem>(&m)) {
        onSystemEvent(*e);
        return;
    }
    if (const auto* e = std::get_if<msg::evTransparent>(&m)) {
        onTransparentEvent(*e);
        return;
    }
    if (const auto* e = std::get_if<msg::evMsgBox>(&m)) {
        onMsgBox(*e);
        return;
    }
}

void Application::onTouch(const msg::evTouch& e) {
    if (msgBox.isActive())
        return;
    if (Page* const p = getPage(e.page_id))
        p->onTouch(e);
}

void Application::onTouchXY(const msg::evTouchXY& e) {
    if (msgBox.isActive())
        msgBox.onTouchXY(e);
}

void Application::onMsgBox(const msg::evMsgBox& e) noexcept {
    if (e.comp_id != 0u) {
        if (Component* const c = getComponent(e.page_id, e.comp_id)) {
            c->onMsgBox(e);
            return;
        }
    }
    if (Page* const p = getPage(e.page_id)) {
        p->onMsgBox(e);
        if (e.comp_id == 0u)
            return;
    }
}

void Application::onPageChange(uint8_t page_id) noexcept {
    const uint8_t prev = _currentPageId;
    if (prev != page_id) {
        if (Page* const old_page = getPage(prev))
            old_page->onExit();
    }
    _currentPageId = page_id;
    if (prev != page_id) {
        if (Page* const new_page = getPage(page_id))
            new_page->onLoad();
    }
}

void Application::clearErrors() noexcept {
    _lastError = msg::Status{};
    _lastErrorPage = 0u;
    _lastErrorComp = 0u;
    _stream.clearErrors();
    _gateway.clearError();
    _session.clearError();
}

void Application::dispatchSysVarResponse(uint8_t tag, const msg::getNumeric& response) noexcept {
    switch (static_cast<SysVarTag>(tag)) {
    case SysVarTag::Bkcmd: bkcmd.applyResponse(response); break;
    case SysVarTag::Sys0:
    case SysVarTag::Sys1:
    case SysVarTag::Sys2:
        sys[static_cast<size_t>(tag) - static_cast<size_t>(SysVarTag::Sys0)].applyResponse(response);
        break;
    case SysVarTag::Pio0:
    case SysVarTag::Pio1:
    case SysVarTag::Pio2:
    case SysVarTag::Pio3:
    case SysVarTag::Pio4:
    case SysVarTag::Pio5:
    case SysVarTag::Pio6:
    case SysVarTag::Pio7:
        pio[static_cast<size_t>(tag) - static_cast<size_t>(SysVarTag::Pio0)].applyResponse(response);
        break;
    case SysVarTag::Tch0:
    case SysVarTag::Tch1:
    case SysVarTag::Tch2:
    case SysVarTag::Tch3:
        touch.tch[static_cast<size_t>(tag) - static_cast<size_t>(SysVarTag::Tch0)].applyResponse(response);
        break;
    default: break;
    }
}

} // namespace nex
