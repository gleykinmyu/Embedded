/**
 * @file nexApplication.cpp
 *
 * Реализация `Application` **без очереди команд**: одна сессия UART (`UartSession`), прямой вызов `Gateway::pushCommand`
 * и неблокирующая передача через `Gateway::transmit()`. Поддерживаются:
 * - обычные команды (`pushCommand`) — после отправки кадра ожидается один `StatusResponse`;
 * - `requestNumericAttributeGet` / `requestStringAttributeGet` — ожидание 0x71 / 0x70 либо статуса ошибки.
 *
 * Ответы панели обрабатываются только когда исходящий кадр полностью ушёл (`Gateway::isTxIdle()`), чтобы не
 * перепутать ответ с предыдущей сессией.
 */

#include "nexApplication.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <variant>

namespace nex {

namespace {

[[nodiscard]] constexpr bool statusIsOk(msg::StatusResponse::Code c) noexcept {
    return c == msg::StatusResponse::Code::InstructionSuccess;
}

} // namespace

Application::Application(BIF::IByteStream& stream) noexcept
    : _root(static_cast<Application&>(*this), cmd::kEmptyCompLexeme, Component::Type::Variable, 0u)
    , _gateway(stream)
    , ep(*this)
    , fs(*this)
    , cs(*this) {}

void Application::Error(const char* format, ...) noexcept {
    std::va_list ap;
    va_start(ap, format);
    std::vprintf(format, ap);
    va_end(ap);
}

PageBase* Application::page(uint8_t id) noexcept {
    return (id < kMaxPages) ? _pages[id] : nullptr;
}

const PageBase* Application::page(uint8_t id) const noexcept {
    return (id < kMaxPages) ? _pages[id] : nullptr;
}

void Application::registerPage(PageBase& page) noexcept {
    const unsigned id = page.ID;
    const Literal& pageName = page.name;
    if (id >= kMaxPages) {
        Error("nex: registerPage: page id=%u out of range (need < %u) name=%.*s\n", id, kMaxPages,
            static_cast<int>(pageName.len), pageName.data);
        return;
    }
    PageBase* const slot = _pages[id];
    if (slot != nullptr && slot != &page) {
        Error("nex: registerPage: page id=%u already registered name=%.*s\n", id, static_cast<int>(pageName.len),
            pageName.data);
        return;
    }
    _pages[id] = &page;
}

void Application::resetGetPending() noexcept {
    _pendingGetKind = PendingGetKind::None;
    _pendingNumeric = {};
    _pendingString = {};
}

void Application::finishUartSession(msg::AppEvent::Outcome outcome, uint8_t code) noexcept {
    _uartSession = UartSession::Idle;
    resetGetPending();
    const msg::AppEvent ev = msg::AppEvent::commandResult(0u, 0u, 0u, outcome, code);
    Message m{ev};
    onAppEvent(ev, m);
}

bool Application::transmitOrAbort() noexcept {
    if (_gateway.transmit())
        return true;
    if (_uartSession != UartSession::Idle || _pendingGetKind != PendingGetKind::None) {
        const msg::AppEvent ev = msg::AppEvent::commandResult(0u, 0u, 0u, msg::AppEvent::Outcome::TxError, 0u);
        Message m{ev};
        _uartSession = UartSession::Idle;
        resetGetPending();
        onAppEvent(ev, m);
    }
    return false;
}

bool Application::tryHandleUartSession(const Message& m) noexcept {
    if (!_gateway.isTxIdle())
        return false;

    switch (_uartSession) {
    case UartSession::AwaitingNumericGet: {
        bool completesGet = false;
        bool isStatus = false;
        uint8_t statusCode = 0u;
        std::visit(
            [&completesGet, &isStatus, &statusCode](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, msg::StringResponse>) {
                    completesGet = true;
                } else if constexpr (std::is_same_v<T, msg::NumericResponse>) {
                    completesGet = true;
                } else if constexpr (std::is_same_v<T, msg::StatusResponse>) {
                    completesGet = true;
                    isStatus = true;
                    statusCode = static_cast<uint8_t>(arg.status);
                }
            },
            m);
        if (!completesGet)
            return false;
        if (isStatus) {
            finishUartSession(msg::AppEvent::Outcome::DeviceStatus, statusCode);
            return false;
        }
        if (_pendingGetKind != PendingGetKind::Numeric)
            return false;
        const auto* const nr = std::get_if<msg::NumericResponse>(&m);
        if (nr == nullptr || _pendingNumeric.dst == nullptr || _pendingNumeric.size == 0u) {
            finishUartSession(msg::AppEvent::Outcome::TxError);
            return false;
        }
        const uint8_t tag = _pendingNumeric.tag;
        const std::size_t n = std::min(_pendingNumeric.size, sizeof(int32_t));
        std::memcpy(_pendingNumeric.dst, &nr->value, n);
        onNumericResponse(*nr, tag);
        finishUartSession(msg::AppEvent::Outcome::Completed);
        return true;
    }

    case UartSession::AwaitingStringGet: {
        bool completesGet = false;
        bool isStatus = false;
        uint8_t statusCode = 0u;
        std::visit(
            [&completesGet, &isStatus, &statusCode](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, msg::StringResponse>) {
                    completesGet = true;
                } else if constexpr (std::is_same_v<T, msg::NumericResponse>) {
                    completesGet = true;
                } else if constexpr (std::is_same_v<T, msg::StatusResponse>) {
                    completesGet = true;
                    isStatus = true;
                    statusCode = static_cast<uint8_t>(arg.status);
                }
            },
            m);
        if (!completesGet)
            return false;
        if (isStatus) {
            finishUartSession(msg::AppEvent::Outcome::DeviceStatus, statusCode);
            return false;
        }
        if (_pendingGetKind != PendingGetKind::String)
            return false;
        const auto* const sr = std::get_if<msg::StringResponse>(&m);
        if (sr == nullptr || _pendingString.dst == nullptr || _pendingString.cap == 0u) {
            finishUartSession(msg::AppEvent::Outcome::TxError);
            return false;
        }
        const uint8_t tag = _pendingString.tag;
        const uint32_t cap = _pendingString.cap;
        const uint32_t maxCopy = cap > 0u ? cap - 1u : 0u;
        const uint32_t ncopy = std::min<uint32_t>(sr->length, maxCopy);
        if (ncopy > 0u)
            std::memcpy(_pendingString.dst, sr->chars, ncopy);
        if (cap > 0u)
            _pendingString.dst[ncopy] = '\0';
        onStringResponse(*sr, tag);
        finishUartSession(msg::AppEvent::Outcome::Completed);
        return true;
    }

    case UartSession::AwaitingStatus: {
        const auto* const st = std::get_if<msg::StatusResponse>(&m);
        if (st == nullptr)
            return false;
        if (!statusIsOk(st->status)) {
            finishUartSession(msg::AppEvent::Outcome::DeviceStatus, static_cast<uint8_t>(st->status));
            return false;
        }
        finishUartSession(msg::AppEvent::Outcome::Completed);
        return false;
    }

    case UartSession::Idle:
    default:
        return false;
    }
}

bool Application::update() noexcept {
    bool any = false;
    if (!transmitOrAbort())
        any = true;

    Message m{};
    while (_gateway.receive(m)) {
        any = true;
        const bool suppress = tryHandleUartSession(m);
        if (!suppress)
            routeInboundMessage(m);
    }
    (void)transmitOrAbort();
    return any;
}

void Application::routeInboundMessage(const Message& m) noexcept {
    std::visit(
        [this, &m](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, msg::TouchEvent>) {
                onTouch(arg);
                if (PageBase* const p = page(arg.page_id))
                    p->dispatchTouch(arg);
            } else if constexpr (std::is_same_v<T, msg::TouchXYEvent>) {
                onTouchXY(arg);
            } else if constexpr (std::is_same_v<T, msg::PageEvent>) {
                _currentPageId = arg.page_id;
                onPageChange(arg.page_id);
            } else if constexpr (std::is_same_v<T, msg::SystemEvent>) {
                onSystemEvent(arg);
            } else if constexpr (std::is_same_v<T, msg::AppEvent>) {
                onAppEvent(arg, m);
            } else if constexpr (std::is_same_v<T, msg::StatusResponse>) {
                onStatusResponse(arg);
            } else if constexpr (std::is_same_v<T, msg::NumericResponse>) {
                onNumericResponse(arg, 0);
            } else if constexpr (std::is_same_v<T, msg::StringResponse>) {
                onStringResponse(arg, 0);
            }
        },
        m);
}

bool Application::requestNumericAttributeGet(const cmd::Get& get, uint8_t* valueDst, std::size_t valueSizeBytes,
    uint8_t tag) noexcept {
    if (_uartSession != UartSession::Idle || _pendingGetKind != PendingGetKind::None || valueDst == nullptr
        || valueSizeBytes == 0u)
        return false;
    if (!_gateway.isTxIdle())
        return false;
    const cmd::Get g = get;
    if (!_gateway.pushCommand(g))
        return false;
    _pendingNumeric.dst = valueDst;
    _pendingNumeric.size = valueSizeBytes;
    _pendingNumeric.tag = tag;
    _pendingGetKind = PendingGetKind::Numeric;
    _uartSession = UartSession::AwaitingNumericGet;
    return true;
}

bool Application::requestStringAttributeGet(const cmd::Get& get, char* stringDst, uint32_t stringBufBytes,
    uint8_t tag) noexcept {
    if (_uartSession != UartSession::Idle || _pendingGetKind != PendingGetKind::None || stringDst == nullptr
        || stringBufBytes == 0u)
        return false;
    if (!_gateway.isTxIdle())
        return false;
    const cmd::Get g = get;
    if (!_gateway.pushCommand(g))
        return false;
    _pendingString.dst = stringDst;
    _pendingString.cap = stringBufBytes;
    _pendingString.tag = tag;
    _pendingGetKind = PendingGetKind::String;
    _uartSession = UartSession::AwaitingStringGet;
    return true;
}

bool Application::calibrateTouch() noexcept {
    return pushCommand(cmd::System::touchJ());
}

bool Application::restart() noexcept {
    return pushCommand(cmd::System::restart());
}

bool Application::setRandGen(int32_t minVal, int32_t maxVal) noexcept {
    return pushCommand(cmd::System::randset(minVal, maxVal));
}

bool Application::play(uint32_t channel, uint32_t resourceId, uint32_t loop01) noexcept {
    return pushCommand(cmd::Play(channel, resourceId, loop01));
}

} // namespace nex
