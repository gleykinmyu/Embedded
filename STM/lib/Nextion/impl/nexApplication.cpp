// nexApplication.cpp — очередь исходящих команд в Session, по одной активной транзакции на UART.
//
// Исходящий поток: enqueue только в очередь; передача в sessionTransmit (sessionBegin + transmit).
// После успешного pushCommand — _session.clearTimeout(), чтобы таймаут ответа не тянулся с прошлой команды.
//
// Входящий поток в update(now_ms): сначала checkSessionTimeout, затем TX, при `isTxIdle()` — дедлайн ожидания
// ответа или при `!bkcmdEnabled()` и `AwaitingStatus` — сразу sessionEnd(true); цикл receive — dispatchResponse / dispatchEvent.
// txIdle: пока запрос не ушёл целиком, ответ к транзакции не привязывают.
//
// Таймаут: после полного TX (isTxIdle) для get/status один раз ставится дедлайн; просрочка — SessionTimeout
// и sessionEnd(false). Если на панели выключен `bkcmd`, вызовите `setBkcmdEnabled(false)`: для `AwaitingStatus`
// после полного TX сессия закрывается успешно без ожидания status.

#include "nexApplication.hpp"
#include "../core/nexErrors.hpp"

#include <type_traits>
#include <variant>

namespace nex {

//=============================================================================

Application::Application(BIF::IByteStream& stream, uint16_t panel_width, uint16_t panel_height) noexcept
    : ep(*this)
    , fs(*this)
    , cs(*this)
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
        if (active.isStatusResponse() && !_bkcmdEnabled)
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
            if (Component* const c = getComponent(active.page_id, active.comp_id))
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
    std::visit(
        [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, msg::evTouch>) {
                dispatchTouch(arg);
            } else if constexpr (std::is_same_v<T, msg::evTouchXY>) {
                if (_touchBlocked) {
                    restart();
                    return;
                }
                onTouchXY(arg);
            } else if constexpr (std::is_same_v<T, msg::evPage>) {
                dispatchPageChange(arg);
            } else if constexpr (std::is_same_v<T, msg::evSystem>) {
                onSystemEvent(arg);
            } else if constexpr (std::is_same_v<T, msg::evTransparent>) {
                onTransparentEvent(arg);
            } else if constexpr (std::is_same_v<T, msg::Status>) {
                dispatchError(arg, 0u, 0u);
            }
        },
        m);
}

void Application::dispatchTouch(const msg::evTouch& e) noexcept {
    if (_touchBlocked) {
        restart();
        return;
    }
    onTouch(e);
    if (Page* const p = page(e.page_id))
        p->onTouch(e);
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
    onError(status, page_id, comp_id);
    if (page_id == 0u && comp_id == 0u)
        return;
    if (Page* const p = page(page_id))
        p->onError(status, comp_id);
}

} // namespace nex