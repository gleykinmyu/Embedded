// nexApplication.cpp — очередь исходящих команд в Session, по одной активной транзакции на UART.
//
// Исходящий поток: enqueue только в очередь; передача в sessionTransmit (sessionBegin + transmit).
// После успешного pushCommand — _session.clearTimeout(), чтобы таймаут ответа не тянулся с прошлой команды.
//
// Входящий поток в update(now_ms): сначала checkSessionTimeout, затем TX, при `isTxIdle()` — дедлайн ожидания
// ответа; для `AwaitingStatus` при `bkcmd != Always` — сразу sessionEnd(true) (status не гарантирован).
// Цикл receive — dispatchResponse / dispatchEvent. txIdle: пока запрос не ушёл целиком, ответ не привязывают.
//
// Таймаут: после полного TX (isTxIdle) для get/status один раз ставится дедлайн; просрочка — SessionTimeout
// и sessionEnd(false). Status по `AwaitingStatus` ждём только при `bkcmd == Always`; иначе — успех без ожидания.

#include "nexApplication.hpp"
#include "../core/nexErrors.hpp"

#include <variant>

namespace nex {

//=============================================================================

Application::Application(BIF::IByteStream& stream, uint16_t panel_width, uint16_t panel_height) noexcept
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
    , _screen(panel_width, panel_height)
    , _stream(stream)
    , _gateway(stream)
{}

Page* Application::page(uint8_t id) noexcept {
    return (id < kMaxPages) ? _pages[id] : nullptr;
}

const Page* Application::page(uint8_t id) const noexcept {
    return (id < kMaxPages) ? _pages[id] : nullptr;
}

Component* Application::getComponent(uint8_t page_id, uint8_t comp_id) noexcept {
    Page* const p = page(page_id);
    if (p == nullptr)
        return nullptr;
    return p->getComponent(comp_id);
}

void Application::registerPage(Page& page) noexcept {
    const unsigned id = page.ID;
    if (id >= kMaxPages) {
        reportRegisterError(Status::PageRegisterFailed, RegisterError::PageIdOutOfRange, static_cast<uint8_t>(id),
            0u);
        return;
    }
    Page* const slot = _pages[id];
    if (slot != nullptr && slot != &page) {
        reportRegisterError(Status::PageRegisterFailed, RegisterError::PageSlotOccupied, static_cast<uint8_t>(id),
            0u);
        return;
    }
    _pages[id] = &page;
}

//=============================================================================
//Рабочий цикл сбора и обработки событий
//=============================================================================

//Поставить транзакцию в очередь; UART — в update(now_ms) или после завершения сессии.
//TODO(QueueFull): при переполнении — отложить tx (буфер ожидания), по session timeout / completeTransaction
//повторить tryEnqueue, чтобы команда не терялась после EnqueueRejected.
void Application::enqueue(Transaction tx) noexcept {
    if (_session.tryEnqueue(tx))
        return;
    handleEnqueueFailure(tx);
}

bool Application::update(uint32_t now_ms) noexcept {
    checkSessionTimeout(now_ms);
    bool any = false;

    if (!sessionTransmit())
        any = true;

    if (_gateway.isTxIdle() && !_session.isIdle()) {
        const Transaction& active = _session.active();
        if (active.isStatusResponse() && bkcmd != BkCmd::Always)
            sessionEnd(true);
        else
            _session.startResponseTimeout(now_ms, kDefaultGetResponseTimeoutMs);
    }

    Message m{};
    while (_gateway.receive(m)) {
        any = true;
        if (dispatchResponse(m, _gateway.isTxIdle()))
            continue;
        dispatchEvent(m);
    }
    if (_stream.getStatus() != BIF::IByteStream::Status::OK || _gateway.getStatus() != Gateway::Status::OK) {
        handleGatewayStreamFailure(Status::GatewayReceiveFailed);
        any = true;
    }

    return any;
}

//=============================================================================
//Сессия: активная транзакция и очередь, внутренние функции для управления сессией
//=============================================================================

//При idle-сессии и свободном TX взять голову очереди, `pushCommand`. false — activate/push провал;
//после успешного push — `_session.clearTimeout()` под новый дедлайн ответа.
void Application::sessionBegin() noexcept {
    if (!_session.hasQueued() || !_session.isIdle() || !_gateway.isTxIdle())
        return;

    if (!_session.activateHead()) {
        handleSessionActivateFailure();
        return;
    }

    if (!_gateway.pushCommand(_session.active().command())) {
        handleGatewayPushFailure();
        return;
    }
    _session.clearTimeout();
}

void Application::sessionEnd(bool Success) noexcept {
    (void)Success;
    _session.completeTransaction();
}

//Шаг TX: при необходимости `sessionBegin`, затем `Gateway::transmit`.
//true — нечего слать, байт ушёл или write==0 при OK потока (отложить).
//false — ошибка UART при TX: handleGatewayStreamFailure (деталь stream/gateway, recovery, onError).
bool Application::sessionTransmit() noexcept {
    sessionBegin();

    if (_gateway.transmit())
        return true;

    handleGatewayStreamFailure(Status::GatewayTransmitFailed);
    return false;
}

void Application::checkSessionTimeout(uint32_t now_ms) noexcept {
    if (!_session.isResponseTimedOut(now_ms))
        return;
    const Transaction& active = _session.active();
    dispatchError(makeAppError(Status::SessionTimeout), active.page_id, active.comp_id);
    sessionEnd(false);
}

//=============================================================================
//Dispatch (входящие кадры / ошибки)
//=============================================================================

bool Application::dispatchResponse(const Message& m, bool txIdle) noexcept {
    if (!txIdle || _session.isIdle())
        return false;

    const Transaction& active = _session.active();

    switch (active.state) {
    case Transaction::State::AwaitingNumericGet: {
        if (const auto* st = std::get_if<msg::Status>(&m)) {
            dispatchError(*st, active.page_id, active.comp_id);
            sessionEnd(false);
            return true;
        }
        if (const auto* nr = std::get_if<msg::getNumeric>(&m)) {
            if (active.page_id == kSysVarRoutePageId && active.comp_id == kSysVarRouteCompId) {
                dispatchSysVarResponse(active.tag, *nr);
                onSysResponse(active.tag, *nr);
            } else if (Component* const c = getComponent(active.page_id, active.comp_id))
                c->onResponse(active.tag, *nr);
            sessionEnd(true);
            return true;
        }
        return false;
    }

    case Transaction::State::AwaitingStringGet: {
        if (const auto* st = std::get_if<msg::Status>(&m)) {
            dispatchError(*st, active.page_id, active.comp_id);
            sessionEnd(false);
            return true;
        }
        if (const auto* sr = std::get_if<msg::getString>(&m)) {
            if (Component* const c = getComponent(active.page_id, active.comp_id))
                c->onResponse(active.tag, *sr);
            sessionEnd(true);
            return true;
        }
        return false;
    }

    case Transaction::State::AwaitingStatus: {
        const auto* const st = std::get_if<msg::Status>(&m);
        if (st == nullptr)
            return false;

        dispatchError(*st, active.page_id, active.comp_id);
        if (!st->isOK()) {
            sessionEnd(false);
            return true;
        }
        sessionEnd(true);
        return true;
    }

    case Transaction::State::Idle:
    default:
        return false;
    }
}

void Application::dispatchEvent(const Message& m) noexcept {
    if (std::get_if<msg::evTouch>(&m) || std::get_if<msg::evTouchXY>(&m)) {
        dispatchTouch(m);
        return;
    }
    if (const auto* e = std::get_if<msg::evPage>(&m)) {
        dispatchPageChange(*e);
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
    if (const auto* e = std::get_if<msg::Status>(&m))
        dispatchError(*e, 0u, 0u);
}

void Application::dispatchTouch(const Message& m) noexcept {
    if (const auto* e = std::get_if<msg::evTouch>(&m)) 
    {
        if (!msgBox.isActive()) {
            onTouch(*e);
            if (Page* const p = page(e->page_id))
                p->onTouch(*e);
            return;
        }
    }
    if (const auto* e = std::get_if<msg::evTouchXY>(&m)) {
        if (msgBox.isActive())
            msgBox.onTouchXY(*e);
        else
            onTouchXY(*e);
    }
}

void Application::dispatchPageChange(const msg::evPage& e) noexcept {
    const uint8_t prev = _currentPageId;
    const uint8_t next = e.page_id;
    if (prev != next) {
        if (Page* const old_page = page(prev))
            old_page->onExit();
    }
    _currentPageId = next;
    onPageChange(next);
    if (prev != next) {
        if (Page* const new_page = page(next))
            new_page->onLoad();
    }
}

void Application::dispatchError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept {
    _lastError = status;
    _lastErrorPage = page_id;
    _lastErrorComp = comp_id;
    onError(status, page_id, comp_id);
    if (page_id == 0u && comp_id == 0u)
        return;
    if (Page* const p = page(page_id))
        p->onError(status, comp_id);
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