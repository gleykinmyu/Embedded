#include "../nexObjManager.hpp"

#include <cstdint>
#include <type_traits>

namespace nex {

ObjManager::ObjManager(BIF::IByteStream& stream) noexcept : _gateway(stream) {}

void ObjManager::completeCurrentCommand(msg::CommandResultEvent::Outcome outcome, uint8_t code) noexcept {
    const CommandMeta meta = (_cmdFifo.peekMeta() != nullptr) ? *_cmdFifo.peekMeta() : CommandMeta{};
    dispatchMessage(msg::CommandResultEvent{meta.page_id, meta.component_id, meta.token, outcome, code});
    _cmdFifo.pop();
    _sendState = SendState::Idle;
}

bool ObjManager::update(uint32_t nowMs) noexcept {
    // Дозаписать текущий TX-кадр (если есть); без этого `isTxIdle` не станет true.
    if (!_gateway.transmit()) {
        // Ошибка потока во время дозаписи: текущую команду считаем сорванной.
        if (_cmdFifo.peek() != nullptr)
            completeCurrentCommand(msg::CommandResultEvent::Outcome::TxError);
        else
            _sendState = SendState::Idle;
        return false;
    }
    
    processCommandQueue(nowMs);

    bool hadMessage = false;
    Message m{};
    while (_gateway.receive(m)) {
        hadMessage = true;
        onReceiveWhileAwaitingCommand(m);
        processCommandQueue(nowMs);
        dispatchMessage(m);
    }
    return hadMessage;
}

void ObjManager::processCommandQueue(uint32_t nowMs) noexcept {
    // За один вызов: по состоянию `_sendState` либо выходим (ждём UART/ответ/данные),
    // либо переводим автомат дальше и `continue` — пока не сделаем «шаг вперёд» или не упрёмся в ожидание.
    for (;;) {
        const Command* const c = _cmdFifo.peek();
        switch (_sendState) 
        {
        case SendState::AwaitingGetReply:
        case SendState::AwaitingTransparentReady:
        case SendState::AwaitingTransparentComplete:
            if (_getTimeoutMs != 0u && nowMs != 0u && nowMs >= _getDeadlineMs) 
            {
                completeCurrentCommand(msg::CommandResultEvent::Outcome::Timeout);
                continue;
            }
            return;

        case SendState::SendingHead:
            if (!_gateway.isTxIdle())
                return;
            if (c == nullptr) 
            {
                _sendState = SendState::Idle;
                continue;
            } 
            else if (c->hasFlag(Command::Flag::NumericResponce) || c->hasFlag(Command::Flag::StringResponse)) 
            {
                _sendState = SendState::AwaitingGetReply;
                _getDeadlineMs = (_getTimeoutMs != 0u && nowMs != 0u) ? nowMs + _getTimeoutMs : UINT32_MAX;
                return;
            } 
            else if (c->hasFlag(Command::Flag::TransparentReadyToReceive)) 
            {
                _sendState = SendState::AwaitingTransparentReady;
                _getDeadlineMs = (_getTimeoutMs != 0u && nowMs != 0u) ? nowMs + _getTimeoutMs : UINT32_MAX;
                return;
            }
            completeCurrentCommand(msg::CommandResultEvent::Outcome::Completed);
            continue;

        case SendState::TransparentPayload:
            if (c == nullptr) 
            {
                _sendState = SendState::Idle;
                continue;
            } 
            else if (_transparentSent >= c->transparentPayloadBytes()) 
            {
                _sendState = SendState::AwaitingTransparentComplete;
                _getDeadlineMs = (_getTimeoutMs != 0u && nowMs != 0u) ? nowMs + _getTimeoutMs : UINT32_MAX;
                return;
            }
            return;

        case SendState::Idle:
            if (c == nullptr || !_gateway.isTxIdle())
                return;

            if (!_gateway.pushCommand(*c)) {
                // Если при свободном TX не удалось поставить голову очереди в отправку
                // (например, serialize=false), не блокируем очередь "вечной" командой.
                completeCurrentCommand(msg::CommandResultEvent::Outcome::QueueRejected);
                continue;
            }
            
            _sendState = SendState::SendingHead;
            continue;
        }
    }
}

void ObjManager::onReceiveWhileAwaitingCommand(const Message& m) noexcept {
    const SendState state = _sendState;
    if (state != SendState::AwaitingGetReply
        && state != SendState::AwaitingTransparentReady
        && state != SendState::AwaitingTransparentComplete) {
        return;
    }

    // NIS: get завершается 0x70/0x71 или status; transparent — 0xFE (ready) и 0xFD (complete), status тоже фатален.
    bool completesGet = false;
    bool isStatus = false;
    bool gotTransparentReady = false;
    bool gotTransparentComplete = false;
    uint8_t statusCode = 0u;
    std::visit(
        [&completesGet, &isStatus, &gotTransparentReady, &gotTransparentComplete, &statusCode](auto&& arg) 
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, msg::StringResponse>) {
                completesGet = true;
                return;
            }
            if constexpr (std::is_same_v<T, msg::NumericResponse>) {
                completesGet = true;
                return;
            }
            if constexpr (std::is_same_v<T, msg::StatusResponse>) {
                completesGet = true;
                isStatus = true;
                statusCode = static_cast<uint8_t>(arg.status);
                return;
            }
            if constexpr (std::is_same_v<T, msg::SystemEvent>) {
                gotTransparentReady = (arg.code == msg::SystemEvent::Code::TransparentReadyToReceive);
                gotTransparentComplete = (arg.code == msg::SystemEvent::Code::TransparentBlockComplete);
                return;
            }
            return;
        },
        m);

    if (state == SendState::AwaitingGetReply && completesGet) {
        // Маршрутизируем ответ текущей get-команды в компонент-инициатор; разбор полезной нагрузки — на стороне компонента.
        if (const CommandMeta* const meta = _cmdFifo.peekMeta()) {
            if (Page* const p = _pages[meta->page_id])
                p->dispatchMessageToComponent(meta->component_id, m);
        }

        if (isStatus)
            completeCurrentCommand(msg::CommandResultEvent::Outcome::DeviceStatus, statusCode);
        else
            completeCurrentCommand(msg::CommandResultEvent::Outcome::Completed);
        return;
    }

    if ((state == SendState::AwaitingTransparentReady || state == SendState::AwaitingTransparentComplete) && isStatus) {
        completeCurrentCommand(msg::CommandResultEvent::Outcome::DeviceStatus, statusCode);
        return;
    }

    if (state == SendState::AwaitingTransparentReady && gotTransparentReady) {
        _transparentSent = 0u;
        _sendState = SendState::TransparentPayload;
        return;
    }

    if (state == SendState::AwaitingTransparentComplete && gotTransparentComplete) {
        completeCurrentCommand(msg::CommandResultEvent::Outcome::Completed);
    }
}

bool ObjManager::writeTransparent(const uint8_t* data, size_t len) noexcept {
    if (_sendState != SendState::TransparentPayload || data == nullptr || len == 0u)
        return false;
    const Command* const c = _cmdFifo.peek();
    if (c == nullptr || !c->hasFlag(Command::Flag::TransparentReadyToReceive))
        return false;
    const uint32_t need = c->transparentPayloadBytes();
    if (_transparentSent >= need)
        return false;
    const uint32_t remain = need - _transparentSent;
    if (len > static_cast<size_t>(remain))
        return false;
    if (!_gateway.writeTransparentRaw(data, len))
        return false;

    _transparentSent += static_cast<uint32_t>(len);
    // Сразу попробовать следующую команду (таймаут get с nowMs==0 здесь не трогаем).
    if (_transparentSent >= need)
        processCommandQueue(0u);
    return true;
}

bool ObjManager::registerPage(Page& page) noexcept {
    const uint8_t id = page.ID;
    Page* const slot = _pages[id];
    if (slot != nullptr && slot != &page)
        return false;
    _pages[id] = &page;
    return true;
}

void ObjManager::dispatchMessage(const Message& m) noexcept {
    std::visit(
        [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, msg::TouchCompEvent>) {
                if (Page* const p = _pages[arg.page_id])
                    p->dispatchTouch(arg);
            } else if constexpr (std::is_same_v<T, msg::PageEvent>) {
                _currentPageId = arg.page_id;
            } else if constexpr (std::is_same_v<T, msg::CommandResultEvent>) {
                if (Page* const p = _pages[arg.page_id])
                    p->dispatchCommandResult(arg);
            }
        },
        m);
}

} // namespace nex
