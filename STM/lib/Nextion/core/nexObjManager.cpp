#include "nexObjManager.hpp"

#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace nex {

ObjManager::ObjManager(BIF::IByteStream& stream) noexcept : _gateway(stream) {}

void ObjManager::tick(uint32_t nowMs) noexcept {
    // Дозаписать текущий TX-кадр (если есть); без этого `isTxIdle` не станет true.
    (void)_gateway.transmit();
    processCommandQueue(nowMs);
}

void ObjManager::processCommandQueue(uint32_t nowMs) noexcept {
    // За один вызов: по состоянию `_sendState` либо выходим (ждём UART/ответ/данные),
    // либо переводим автомат дальше и `continue` — пока не сделаем «шаг вперёд» или не упрёмся в ожидание.
    for (;;) {
        switch (_sendState) {
        case SendState::AwaitingGetReply:
            if (_getTimeoutMs != 0u && nowMs != 0u && nowMs >= _getDeadlineMs) {
                _cmdFifo.pop();
                _sendState = SendState::Idle;
                continue;
            }
            return;

        case SendState::SendingHead:
            if (!_gateway.isTxIdle())
                return;
            if (const Command* const c = _cmdFifo.peek(); c == nullptr) {
                _sendState = SendState::Idle;
                continue;
            } else if (c->awaitsGetAttributeReply()) {
                _sendState = SendState::AwaitingGetReply;
                _getDeadlineMs = (_getTimeoutMs != 0u && nowMs != 0u) ? nowMs + _getTimeoutMs : UINT32_MAX;
                return;
            } else if (c->hasTransparentPhase()) {
                _sendState = SendState::TransparentPayload;
                _transparentSent = 0u;
                return;
            }
            _cmdFifo.pop();
            _sendState = SendState::Idle;
            continue;

        case SendState::TransparentPayload:
            if (const Command* const c = _cmdFifo.peek(); c == nullptr) {
                _sendState = SendState::Idle;
                continue;
            } else if (_transparentSent >= c->transparentPayloadBytes()) {
                _cmdFifo.pop();
                _sendState = SendState::Idle;
                continue;
            }
            return;

        case SendState::Idle:
            if (_cmdFifo.empty() || !_gateway.isTxIdle())
                return;
            {
                const Command* const c = _cmdFifo.peek();
                const bool ok = c->hasTransparentPhase()
                    ? _gateway.pushTransparentPreamble(static_cast<const TransparentCommand&>(*c))
                    : _gateway.pushCommand(*c);
                if (!ok)
                    return;
            }
            _sendState = SendState::SendingHead;
            continue;
        }
    }
}

void ObjManager::onReceiveWhileAwaitingGet(const Message& m) noexcept {
    if (_sendState != SendState::AwaitingGetReply)
        return;
    // NIS: ответ на get — строка 0x70 или число 0x71; status тоже завершает «транзакцию» (ошибка/успех инструкции).
    const bool completesGet = std::visit(
        [](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, msg::StringResponse>)
                return true;
            if constexpr (std::is_same_v<T, msg::NumericResponse>)
                return true;
            if constexpr (std::is_same_v<T, msg::StatusResponse>)
                return true;
            return false;
        },
        m);
    if (completesGet) {
        _cmdFifo.pop();
        _sendState = SendState::Idle;
    }
}

size_t ObjManager::writeTransparent(const uint8_t* data, size_t len) noexcept {
    if (_sendState != SendState::TransparentPayload || data == nullptr || len == 0u)
        return 0u;
    const Command* const c = _cmdFifo.peek();
    if (c == nullptr || !c->hasTransparentPhase())
        return 0u;
    const uint32_t need = c->transparentPayloadBytes();
    if (_transparentSent >= need)
        return 0u;
    const uint32_t remain = need - _transparentSent;
    const size_t chunk = std::min(len, static_cast<size_t>(remain));
    const size_t w = _gateway.writeTransparentRaw(data, chunk);
    _transparentSent += static_cast<uint32_t>(w);
    // Сразу попробовать следующую команду (таймаут get с nowMs==0 здесь не трогаем).
    if (_transparentSent >= need)
        processCommandQueue(0u);
    return w;
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
            }
        },
        m);
}

bool ObjManager::poll(Message& m, uint32_t nowMs) noexcept {
    // Порядок: обновить TX/очередь → если есть полный RX-кадр — разобрать get → снова очередь → touch/page.
    tick(nowMs);
    if (!_gateway.receive(m))
        return false;
    onReceiveWhileAwaitingGet(m);
    processCommandQueue(nowMs);
    dispatchMessage(m);
    return true;
}

} // namespace nex
