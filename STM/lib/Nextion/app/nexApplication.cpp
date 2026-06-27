#include "nexApplication.hpp"
#include "../core/nexTimeout.hpp"
#include "../core/nexStatusMask.hpp"

#include <variant>

#include "../core/nexDebug.hpp"

namespace nex {

Application::Application(BIF::IByteStream& stream, Rect screen, AppTiming timing) noexcept
    : cs(*this)
    , touch(*this)
    , sleep(*this)
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
    , _clockMsFn(timing.clockMs)
    , _timeoutMs(timing.timeoutMs)
{
    NEX_ASSERT(timing.clockMs != nullptr);
#if defined(NEX_LOG_TICKS)
    detail::setNexLogTickFn(timing.clockMs);
#endif
}

void Application::onTimeout(const Transaction& active) noexcept {
    onStatus(appErrorFrom(Session::Status::Timeout), active.route);
}

void Application::abortSessionFault() noexcept {
    if (const Transaction* const tx = _session.current())
        onStatus(appErrorFrom(_session.getStatus(), _gateway.getStatus()), tx->route);
}

bool Application::tryEnqueue(Transaction tx) noexcept {
    if (_session.tryEnqueue(tx))
        return true;

    const Session::Status st = _session.getStatus();
    if (st != Session::Status::QueueFull)
        onStatus(appErrorFrom(st), tx.route);
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
            onStatus(appErrorFrom(st), tx.route);
            return;
        }

        const std::size_t queued_before = _session.queuedCount();
        update();
        if (_session.queuedCount() < queued_before || _session.isActive())
            stall.start(nowMs(), _timeoutMs);

        if (stall.timedOut(nowMs()))
            break;
    }

    onStatus(appErrorFrom(Session::Status::QueueFull), tx.route);
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

    if (!_session.transmit(_gateway, now_ms, _timeoutMs)) {
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
    // Сначала привязка к активной tx (`dispatchResponse`), иначе — события панели.
    while (_gateway.receive(m)) {
        if (dispatchResponse(m, _gateway.isTxIdle()))
            continue;
        dispatchEvent(m);
    }

    processTransportFault(now_ms);
}

void Application::processTransportFault(uint32_t now_ms) noexcept {
    // Re-TX head при `StreamRxError` + `bkcmd`/`canRetryActive`; иначе `onStatus` и сброс session.
    const Gateway::Status gwSt = _gateway.getStatus();
    const BIF::IByteStream::Status streamSt = _stream.getStatus();

    if (gwSt == Gateway::Status::OK && streamSt == BIF::IByteStream::Status::OK) {
        _rxFaultRetries = 0u;
        if (_lastError.status != msg::Status::Code::Success)
            _lastError = msg::Status{};
        return;
    }

    switch (gwSt) {
    case Gateway::Status::StreamRxError:
        if (_session.canRetryActive(bkcmd) && _rxFaultRetries < kMaxRxFaultRetries) {
            if (!_gateway.isTxIdle())
                return;

            if (streamSt == BIF::IByteStream::Status::DataError)
                _stream.clearErrors();
            _gateway.clearError();

            if (_session.retryActive(_gateway, bkcmd)) {
                _rxFaultRetries++;
                (void)_session.transmit(_gateway, now_ms, _timeoutMs);
                return;
            }
        }

        if (_session.isActive()) {
            abortSessionFault();
            _session.end(false);
            _rxFaultRetries = 0u;
            return;
        }

        if (streamSt == BIF::IByteStream::Status::DataError)
            _stream.clearErrors();
        _gateway.clearError();
        _rxFaultRetries = 0u;
        break;

    case Gateway::Status::StreamTxError:
        if (_session.isActive()) {
            abortSessionFault();
            _session.end(false);
            _rxFaultRetries = 0u;
            return;
        }
        _rxFaultRetries = 0u;
        break;

    case Gateway::Status::RxOverflow:
        _gateway.clearError();
        break;

    case Gateway::Status::OK:
    default:
        _rxFaultRetries = 0u;
        break;
    }

    onStatus(appErrorFrom(gwSt, streamSt));
}

bool Application::dispatchResponse(const Message& m, bool txIdle) noexcept {
    if (!txIdle || !_session.isActive())
        return false;

    const Transaction* const active = _session.current();
    if (active == nullptr)
        return false;

    if (const auto* st = std::get_if<msg::Status>(&m)) {
        // Status вне активной транзакции: не коррелирует с active tx → `onStatus` без route.
        if (!active->correlatesWith(*st, bkcmd)) {
            onStatus(*st);
            return true;
        }
        onStatus(*st, active->route);
        const bool ok = (active->kind == Transaction::Kind::Command) && st->isOK();
        _session.end(ok);
        return true;
    }

    switch (active->kind) {
    case Transaction::Kind::GetNumeric:
        if (const auto* nr = std::get_if<msg::getNumeric>(&m)) {
            onResponse(*nr, active->route, active->tag);
            _session.end(true);
            return true;
        }
        return false;

    case Transaction::Kind::GetString:
        if (const auto* sr = std::get_if<msg::getString>(&m)) {
            onResponse(*sr, active->route, active->tag);
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

    return false;
}

void Application::dispatchEvent(const Message& m) noexcept {
    if (const auto* e = std::get_if<msg::evPage>(&m)) {
        onPageChange(*e);
        return;
    }
    if (const auto* e = std::get_if<msg::Status>(&m)) {
        onStatus(*e);
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
    (void)e;
}

void Application::onTouchXY(const msg::evTouchXY& e) {
    (void)e;
}

void Application::onMsgBox(const msg::evMsgBox& e) noexcept {
    (void)e;
}

void Application::onPageChange(const msg::evPage& e) noexcept {
    _currentPage = e.page;
}

void Application::onStatus(const msg::Status& status, Route route) noexcept {
    if (_lastError.status != msg::Status::Code::Success && status == _lastError && route == _lastErrorRoute)
        return;

    _lastError = status;
    _lastErrorRoute = route;
    printStatusError(status, route);
}

void Application::onResponse(const msg::getNumeric& response, Route route, uint8_t tag) noexcept
{
    if (route.isSysVar()) {
        switch (static_cast<SysVarTag>(tag)) {
        case SysVarTag::Bkcmd:
            bkcmd.applyResponse(response);
            break;
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
        default:
            break;
        }
    }
    (void)response;
    (void)route;
    (void)tag;
}

void Application::onResponse(const msg::getString& response, Route route, uint8_t tag) noexcept
{
    (void)response;
    (void)route;
    (void)tag;
}

void Application::clearErrors() noexcept {
    _lastError = msg::Status{};
    _lastErrorRoute = {};
    _stream.clearErrors();
    _gateway.clearError();
    _session.clearError();
}

} // namespace nex
