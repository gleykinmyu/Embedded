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

#include <variant>

namespace nex {

//=============================================================================

Application::Application(BIF::IByteStream& stream, uint16_t screen_width, uint16_t screen_height,
    CompIdMapTable& id_map_table) noexcept
    : ep(*this)
    , fs(*this)
    , cs(*this)
    , audio(*this)
    , touch(*this)
    , sleep(*this)
    , msgBox(*this)
    , idMap(*this, id_map_table)
    , bkcmd(*this, "bkcmd", SysVarTag::Bkcmd, BkCmd::OnFailure)
    , sys{SysVar<uint32_t>(*this, "sys0", SysVarTag::Sys0), SysVar<uint32_t>(*this, "sys1", SysVarTag::Sys1),
        SysVar<uint32_t>(*this, "sys2", SysVarTag::Sys2)}
    , pio{SysVar<uint8_t>(*this, "pio0", SysVarTag::Pio0), SysVar<uint8_t>(*this, "pio1", SysVarTag::Pio1),
        SysVar<uint8_t>(*this, "pio2", SysVarTag::Pio2), SysVar<uint8_t>(*this, "pio3", SysVarTag::Pio3),
        SysVar<uint8_t>(*this, "pio4", SysVarTag::Pio4), SysVar<uint8_t>(*this, "pio5", SysVarTag::Pio5),
        SysVar<uint8_t>(*this, "pio6", SysVarTag::Pio6), SysVar<uint8_t>(*this, "pio7", SysVarTag::Pio7)}
    , _screen(screen_width, screen_height)
    , _stream(stream)
    , _gateway(stream)
    , _errors(*this, _stream, _gateway, _session)
{}

QueuePolicy Application::queuePolicy(const AppFailure& failure) const noexcept {
    (void)failure;
    return QueuePolicy::None;
}

uint8_t Application::resetActiveRetryLimit() const noexcept {
    return kDefaultResetActiveRetryLimit;
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
    const uint8_t id = page.ID;
    const MISC::RegStatus st = _pageStorage.registerSeq(&page, id);
    if (st != MISC::RegStatus::Ok)
        reportRegisterError(Status::PageRegisterFailed, st, id, 0u);
}

//=============================================================================
//Рабочий цикл сбора и обработки событий
//=============================================================================

//Поставить транзакцию в очередь; UART — в update(now_ms) или после завершения сессии.
//TODO(QueueFull): при переполнении — отложить tx (буфер ожидания), по session timeout / completeTransaction
//повторить tryEnqueue, чтобы команда не терялась после EnqueueRejected.
void Application::enqueue(Transaction tx) noexcept {
    if (_errors.isLinkDown())
        return;
    if (_session.tryEnqueue(tx))
        return;
    _errors.handle(captureEnqueueFailure(tx, _session));
}

bool Application::update(uint32_t now_ms) noexcept {
    idMap.updateDiscovery(now_ms);
    _errors.clearLinkDownIfStreamOk();

    checkSessionTimeout(now_ms);

    if (_errors.isLinkDown()) {
        bool any = false;
        Message m{};
        while (_gateway.receive(m)) {
            any = true;
            if (dispatchResponse(m, _gateway.isTxIdle()))
                continue;
            dispatchEvent(m);
        }
        return any;
    }

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
        _errors.handle(captureGatewayStreamFailure(AppOperation::GatewayReceiveFailed, _session, _stream, _gateway));
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
        _errors.handle(captureSessionActivateFailure(_session));
        return;
    }

    if (!_gateway.pushCommand(_session.active().command())) {
        _errors.handle(captureGatewayPushFailure(_session, _gateway));
        return;
    }
    _session.clearTimeout();
}

void Application::sessionEnd(bool Success) noexcept {
    idMap.onSessionEnd(Success);
    _session.completeTransaction();
    _errors.onSessionEnded();
}

//Шаг TX: при необходимости `sessionBegin`, затем `Gateway::transmit`.
//true — нечего слать, байт ушёл или write==0 при OK потока (отложить).
//false — ошибка UART при TX: handleGatewayStreamFailure (деталь stream/gateway, recovery, onError).
bool Application::sessionTransmit() noexcept {
    sessionBegin();

    if (_gateway.transmit())
        return true;

    _errors.handle(captureGatewayStreamFailure(AppOperation::GatewayTransmitFailed, _session, _stream, _gateway));
    return false;
}

void Application::checkSessionTimeout(uint32_t now_ms) noexcept {
    if (!_session.isResponseTimedOut(now_ms))
        return;
    const Transaction& active = _session.active();
    if (Route::isCompIdMapPoll(active.page_id, active.comp_id)) {
        idMap.onSessionEnd(false);
        sessionEnd(false);
        return;
    }
    _errors.handle(makeSessionTimeout(active));
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
            if (Route::isCompIdMapPoll(active.page_id, active.comp_id)) {
                idMap.onPollResponse(active.tag, *nr);
                sessionEnd(true);
                return true;
            }
            if (Route::isSysVar(active.page_id, active.comp_id)) {
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

void Application::dispatchEvent(const Message& m) noexcept 
{
    if (const auto* e = std::get_if<msg::evPage>(&m)) {
        dispatchPageChange(*e);
        return;
    }
    if (const auto* e = std::get_if<msg::Status>(&m)) {
        dispatchError(*e, 0u, 0u);
        return;
    }
    // Discover: touch/MsgBox/system не мешают опросу; evPage и Status — выше.
    if (idMap.isDiscoveryBusy()) return;

    //=======================================================
    if (std::get_if<msg::evTouch>(&m) || std::get_if<msg::evTouchXY>(&m)) {
        dispatchTouch(m);
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
        dispatchMsgBox(*e);
        return;
    }
}

void Application::dispatchMsgBox(const msg::evMsgBox& e) noexcept {
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
    onMsgBox(e);
}

void Application::dispatchTouch(const Message& m) noexcept {
    if (const auto* e = std::get_if<msg::evTouch>(&m)) 
    {
        if (!msgBox.isActive()) {
            onTouch(*e);
            if (Page* const p = getPage(e->page_id))
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
        if (Page* const old_page = getPage(prev))
            old_page->onExit();
    }
    _currentPageId = next;
    onPageChange(next);
    idMap.onPageChange(next);
    if (prev != next) {
        if (Page* const new_page = getPage(next))
            new_page->onLoad();
    }
}

NexErrors Application::errors() const noexcept {
    NexErrors e{};
    e.stream = _stream.getStatus();
    e.gateway = _gateway.getStatus();
    e.session = _session.getStatus();
    return e;
}

void Application::clearErrors() noexcept {
    _errors.clearErrors();
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