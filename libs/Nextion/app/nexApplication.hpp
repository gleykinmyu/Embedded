#pragma once

#include <cstdint>
#include "../../Interfaces/ibyte_stream.hpp"
#include "../core/nexTypes.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexSession.hpp"
#include "../core/nexMessages.hpp"
#include "nexErrors.hpp"
#include "nexApplicationFacades.hpp"
#include "nexSysVars.hpp"

/** Повторы re-TX head при `Gateway::StreamRxError` (после исходной отправки). */
static constexpr uint8_t kMaxRxFaultRetries = 2u;

namespace nex {

class IPage;

/** clock + session timeout. */
struct AppTiming {
    using ClockMsFn = uint32_t (*)() noexcept;

    static constexpr uint32_t kDefaultTimeoutMs = 500u;

    ClockMsFn clockMs{};
    uint32_t timeoutMs = kDefaultTimeoutMs;
};

/** Runtime + protocol hooks + screen shell. Без page/component registry — см. `IAppUI` / `AppUI<>`. */
class Application {
public:
    using ClockMsFn = AppTiming::ClockMsFn;
    using Timing = AppTiming;

    explicit Application(BIF::IByteStream& stream, Rect screen, AppTiming timing) noexcept;
    virtual ~Application() = default;

    /** Поставить `Transaction` в очередь `Session`; при `QueueFull` — крутит `update()` до stall-timeout (`getTimeout()`). */
    void enqueue(Transaction tx) noexcept;
    /** Неблокирующая постановка: одна попытка без `update()`. `false` при `QueueFull` — без `onStatus`; иная ошибка Session → `onStatus`. */
    bool tryEnqueue(Transaction tx) noexcept;

    /** Один цикл pump: TX/RX `Session`, разбор `Message`, `processTransportFault`. */
    virtual void update() noexcept;

    /** Крутит `update()` пока session idle (`!isActive() && !hasQueued()`).
     *  @return `true` — idle; `false` — stall-timeout без прогресса очереди (`getTimeout()`). */
    [[nodiscard]] bool pumpUntilIdle() noexcept;

    void setTimeout(uint32_t ms) noexcept { _timeoutMs = ms; }
    [[nodiscard]] uint32_t getTimeout() const noexcept { return _timeoutMs; }

    /** Кэш последнего `onStatus` (после dedup в базовом override). */
    [[nodiscard]] const msg::Status& lastError() const noexcept { return _lastError; }
    [[nodiscard]] Route lastErrorRoute() const noexcept { return _lastErrorRoute; }
    [[nodiscard]] uint8_t lastErrorPage() const noexcept { return _lastErrorRoute.page; }
    [[nodiscard]] uint8_t lastErrorComp() const noexcept { return _lastErrorRoute.comp; }

    /** Текущее время (тот же источник, что у Session timeout). */
    [[nodiscard]] uint32_t nowMs() const noexcept { return _clockMsFn(); }

    /** Сброс `_lastError` и ошибок stream/gateway; очередь `Session` не трогается. */
    void clearErrors() noexcept;

    [[nodiscard]] const ScreenLayout& screenLayout() const noexcept { return _screen; }

    /** Команды shell экрана (`rest`, `page`, `sendme`, `ref_*`) — без реестра `IAppUI`. */
    void restartScreen() noexcept;
    void switchPage(uint8_t pageId) noexcept;
    void switchPage(const Literal& pageName) noexcept;
    void switchPage(IPage& page) noexcept;
    void requestCurrentPage() noexcept;
    void refreshPage() noexcept;

    /** Последний `page` из `msg::evPage` (`0xFF` до первого события). */
    [[nodiscard]] uint8_t currentPage() const noexcept { return _currentPage; }

    /** Системные переменные NIS §6: `baud`/`bauds`, `addr`, `dim`/`dims`. */
    void setBaudrate(Baudrate rate) noexcept;
    void setBaudrateDefault(Baudrate rate) noexcept;
    void setAddress(uint16_t address) noexcept;
    void setBrightness(uint8_t level) noexcept;
    void setBrightnessDefault(uint8_t level) noexcept;

    /** Числовое присваивание глобальной переменной NIS (`keybdA.loadpageid.val`, …). */
    void setGlobalVar(const Literal& path, int32_t value) noexcept;

    /** Фасады NIS и MCU-виджеты; дружественные поля, не наследуют `Application`. */
    AppCanvas cs;
    AppTouch touch;
    AppSleep sleep;

    /** Зеркала `get` для `sys*` / `pio*` / `bkcmd`; ответы — `onResponse(..., Route::sysVar(), tag)`. */
    SysVar<BkCmd> bkcmd;
    SysVar<uint32_t> sys[3];
    SysVar<uint8_t> pio[8];

    /** Виртуальные хуки UART; база — no-op или делегирование (`IAppUI`, `msgBox`). */
    virtual void onTouch(const msg::evTouch& e);
    virtual void onTouchXY(const msg::evTouchXY& e);
    virtual void onMsgBox(const msg::evMsgBox& e) noexcept;
    virtual void onPageChange(const msg::evPage& e) noexcept;
    virtual void onSystemEvent(const msg::evSystem&) {}
    virtual void onTransparentEvent(const msg::evTransparent&) {}
    virtual void onResponse(const msg::getNumeric& response, Route route, uint8_t tag) noexcept;
    virtual void onResponse(const msg::getString& response, Route route, uint8_t tag) noexcept;
    /** Истек `pollTimeout` активной tx; база → `onStatus(Timeout, active.route)`. */
    virtual void onTimeout(const Transaction& active) noexcept;
    /** Panel **status** и MCU `AppError`; dedup + `_lastError*` в базовом override. */
    virtual void onStatus(const msg::Status& status, Route route = {}) noexcept;

protected:
    /** `true` — кадр привязан к активной tx и обработан; иначе → `dispatchEvent`. */
    [[nodiscard]] virtual bool dispatchResponse(const Message& m, bool txIdle) noexcept;
    /** События вне активной транзакции: `0x65`, `0x66`, status вне активной транзакции, `evMsgBox`, … */
    virtual void dispatchEvent(const Message& m) noexcept;

private:
    friend class SmartApp;

    void abortSessionFault() noexcept;
    void processTransportFault(uint32_t now_ms) noexcept;
    
    ScreenLayout _screen{};

    BIF::IByteStream& _stream;
    Gateway _gateway;
    Session _session;

    msg::Status _lastError{};
    Route _lastErrorRoute{};
    uint8_t _rxFaultRetries = 0u;
    uint8_t _currentPage = 0xFFu;

    ClockMsFn _clockMsFn;
    uint32_t _timeoutMs = AppTiming::kDefaultTimeoutMs;
};

} // namespace nex
