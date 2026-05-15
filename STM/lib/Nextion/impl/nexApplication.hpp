#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "nexComponentBase.hpp"
#include "../core/nexGateway.hpp"
#include "nexApplicationFacades.hpp"

namespace nex {

/** Число слотов таблицы страниц; `PageBase::ID` обязан быть меньше этого значения. */
static constexpr unsigned kMaxPages = 16u;

/**
 * Приложение Nextion: шлюз UART, таблица страниц, маршрутизация входящих сообщений, фасады исходящих команд.
 * Фантомный `Component` `_root` (пустой `comp` в `TargetAttr`) — для пользовательских `Attribute*` на корне NIS; см. `nisRoot()`.
 *
 * **Исходящий путь (без очереди команд):** одна активная сессия UART — пока не получен ожидаемый ответ панели
 * на последнюю отправленную инструкцию, новая команда или новый `get` не принимаются. Команда сериализуется
 * в `Gateway::pushCommand`, байты уходят в `transmit()` из `update()`. **Transparent (wept/rept/twfile/…)** пока
 * не реализованы — `requestTransparentTransmit` / `requestTransparentReceive` возвращают `false`.
 */
class Application {
private:
    friend class PageBase;
    friend class Attribute;
    friend class AppEeprom;
    friend class AppFileSystem;
    friend class AppCanvas;
    template<typename T>
    friend class AttributeNum;
    template<uint8_t MaxL>
    friend class AttributeString;

    void registerPage(PageBase& page) noexcept;
    /**
     * Доставка входящего `Message` в виртуальные колбэки и `PageBase::dispatchTouch`.
     * Вызывается после `tryHandleUartSession`, если кадр не был полностью обработан там (например, ответ `get`).
     */
    void routeInboundMessage(const Message& m) noexcept;

    Component _root;
    Gateway _gateway;
    PageBase* _pages[kMaxPages]{};
    uint8_t _currentPageId = 0xFF;

    /**
     * Состояние «одной команды в полёте» по UART: не FIFO, а строго последовательный протокол NIS.
     * - `Idle` — можно вызвать `pushCommand` или `request*AttributeGet`, если `Gateway::isTxIdle()`.
     * - `AwaitingStatus` — в буфер TX уже положена обычная команда (не `get`); ждём один `StatusResponse`.
     * - `AwaitingNumericGet` / `AwaitingStringGet` — в полёте `get`; ждём 0x71 / 0x70 или статус ошибки.
     */
    enum class UartSession : uint8_t {
        Idle,
        AwaitingStatus,
        AwaitingNumericGet,
        AwaitingStringGet,
    };
    UartSession _uartSession = UartSession::Idle;

    enum class PendingGetKind : uint8_t { None, Numeric, String };
    PendingGetKind _pendingGetKind = PendingGetKind::None;
    struct {
        uint8_t* dst = nullptr;
        std::size_t size = 0;
        uint8_t tag = 0;
    } _pendingNumeric{};
    struct {
        char* dst = nullptr;
        uint32_t cap = 0;
        uint8_t tag = 0;
    } _pendingString{};

    /** Один тик `Gateway::transmit()`; при ошибке потока сбрасывает сессию и шлёт `onAppEvent(TxError)`. */
    [[nodiscard]] bool transmitOrAbort() noexcept;
    /**
     * Обработка кадра, относящегося к текущей исходящей сессии (статус по обычной команде, ответ по `get`).
     * @return `true`, если числовой/строковый ответ `get` уже обработан (не вызывать повторно `onNumericResponse` / `onStringResponse` в `routeInboundMessage`).
     */
    [[nodiscard]] bool tryHandleUartSession(const Message& m) noexcept;
    /** Сбросить ожидание `get` (поля буфера). */
    void resetGetPending() noexcept;
    /** Завершить сессию UART, сбросить pending `get`, уведомить `onAppEvent(CommandResult)` с нулевыми meta. */
    void finishUartSession(msg::AppEvent::Outcome outcome, uint8_t code = 0) noexcept;

protected:
    /**
     * Отправить обычную команду (всё, кроме `cmd::Get` — см. `request*AttributeGet`).
     * @return `false`, если UART занят, сессия не `Idle` или сериализация не удалась.
     */
    template<typename T>
    [[nodiscard]] bool pushCommand(const T& cmd) noexcept {
        static_assert(std::is_base_of_v<Command, T>, "T must derive from nex::Command");
        static_assert(!std::is_same_v<T, cmd::Get>, "use requestNumericAttributeGet / requestStringAttributeGet");
        if (_uartSession != UartSession::Idle)
            return false;
        if (!_gateway.isTxIdle())
            return false;
        if (!_gateway.pushCommand(cmd))
            return false;
        _uartSession = UartSession::AwaitingStatus;
        return true;
    }

    [[nodiscard]] bool requestNumericAttributeGet(const cmd::Get& get, uint8_t* valueDst, std::size_t valueSizeBytes,
        uint8_t tag = 0) noexcept;

    [[nodiscard]] bool requestStringAttributeGet(const cmd::Get& get, char* stringDst, uint32_t stringBufBytes,
        uint8_t tag = 0) noexcept;

    /** Заглушка: transparent TX не реализован без очереди/сессии; всегда `false`. */
    template<typename T>
    [[nodiscard]] bool requestTransparentTransmit(const T&, const uint8_t*, uint32_t, uint8_t = 0) noexcept {
        return false;
    }

    /** Заглушка: transparent RX не реализован; всегда `false`. */
    template<typename T>
    [[nodiscard]] bool requestTransparentReceive(const T&, uint8_t*, uint32_t, uint8_t = 0) noexcept {
        return false;
    }

public:
    explicit Application(BIF::IByteStream& stream) noexcept;
    virtual ~Application() = default;

    /**
     * Дожимает TX, принимает все готовые RX-кадры, сопоставляет ответы с текущей сессией (`get` / статус).
     * @return `true`, если был прогресс (передача, приём, смена сессии).
     */
    [[nodiscard]] bool update() noexcept;

    [[nodiscard]] uint8_t currentPageId() const noexcept { return _currentPageId; }

    [[nodiscard]] PageBase* page(uint8_t id) noexcept;
    [[nodiscard]] const PageBase* page(uint8_t id) const noexcept;

    virtual void onTouch(const msg::TouchEvent&) {}
    virtual void onTouchXY(const msg::TouchXYEvent&) {}
    virtual void onPageChange(uint8_t) {}
    virtual void onSystemEvent(const msg::SystemEvent&) {}
    virtual void onStatusResponse(const msg::StatusResponse&) {}
    virtual void onNumericResponse(const msg::NumericResponse&, uint8_t tag) {}
    virtual void onStringResponse(const msg::StringResponse&, uint8_t tag) {}
    virtual void onAppEvent(const msg::AppEvent&, const Message&) {}

    virtual void Error(const char* format, ...) noexcept;

    bool calibrateTouch() noexcept;
    bool restart() noexcept;
    bool setRandGen(int32_t minVal, int32_t maxVal) noexcept;
    bool play(uint32_t channel, uint32_t resourceId, uint32_t loop01) noexcept;

    [[nodiscard]] Component& nisRoot() noexcept { return _root; }
    [[nodiscard]] const Component& nisRoot() const noexcept { return _root; }

    AppEeprom ep;
    AppFileSystem fs;
    AppCanvas cs;
};

} // namespace nex
