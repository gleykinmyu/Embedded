#pragma once

#include <cstdint>
#include "../../Interfaces/ibyte_stream.hpp"
#include "../../Interfaces/obj_registry.hpp"
#include "../comp/nexComponentBase.hpp"
#include "../core/nexTypes.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexSession.hpp"
#include "../core/nexMessages.hpp"
#include "nexErrors.hpp"
#include "nexApplicationFacades.hpp"
#include "../comp/nexMsgBox.hpp"
#include "nexSysVars.hpp"

namespace nex {

static constexpr unsigned kMaxPages = 16u; //TODO сделать настраиваемым
static constexpr uint32_t kDefaultTimeoutMs = 500u;

class Application {
public:
    static constexpr uint8_t kSysVarRoutePageId = Route::kSysVarPageId;
    static constexpr uint8_t kSysVarRouteCompId = Route::kSysVarCompId;

    using ClockMsFn = uint32_t (*)() noexcept;

    explicit Application(BIF::IByteStream& stream, Rect screen, ClockMsFn clockMs,
        uint32_t timeout_ms = kDefaultTimeoutMs) noexcept;
    virtual ~Application() = default;

    void enqueue(Transaction tx) noexcept;
    virtual void update() noexcept;

    /** Крутит `update()` пока сессия не простаивает; таймаут без прогресса — `getTimeout()`. */
    void pumpUntilIdle() noexcept;

    void setTimeout(uint32_t ms) noexcept { _timeoutMs = ms; }
    [[nodiscard]] uint32_t getTimeout() const noexcept { return _timeoutMs; }

    [[nodiscard]] const msg::Status& lastError() const noexcept { return _lastError; }
    [[nodiscard]] uint8_t lastErrorPage() const noexcept { return _lastErrorPage; }
    [[nodiscard]] uint8_t lastErrorComp() const noexcept { return _lastErrorComp; }

    /** Текущее время (тот же источник, что у Session timeout). */
    [[nodiscard]] uint32_t nowMs() const noexcept { return _clockMsFn(); }

    void clearErrors() noexcept;

    virtual void onTouch(const msg::evTouch& e);
    virtual void onTouchXY(const msg::evTouchXY& e);
    virtual void onMsgBox(const msg::evMsgBox& e) noexcept;
    virtual void onPageChange(uint8_t page_id) noexcept;
    virtual void onSystemEvent(const msg::evSystem&) {}
    virtual void onTransparentEvent(const msg::evTransparent&) {}
    virtual void onSysResponse(uint8_t tag, const msg::getNumeric& response) noexcept { (void)tag; (void)response; }
    /** Перед `_session.end(false)` при таймауте ответа (кроме CompIdMap в `SmartApp`). */
    virtual void onTimeout(const Transaction& active) noexcept;

    /** Статус с панели (NIS) или фоновое событие. */
    virtual void onError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;

    void restartScreen() noexcept;
    void switchPage(uint8_t pageId) noexcept;
    void requestCurrentPage() noexcept;
    void refreshPage() noexcept;

    [[nodiscard]] uint8_t currentPageId() const noexcept { return _currentPageId; }

    [[nodiscard]] Page* getPage(uint8_t id) noexcept;
    [[nodiscard]] uint8_t pageCount() const noexcept { return _pageStorage.registeredCount(); }
    [[nodiscard]] Component* getComponent(uint8_t page_id, uint8_t comp_id) noexcept;

    [[nodiscard]] const ScreenLayout& screenLayout() const noexcept { return _screen; }

    void setBaudrate(Baudrate rate) noexcept;
    void setBaudrateDefault(Baudrate rate) noexcept;
    void setAddress(uint16_t address) noexcept;
    void setBrightness(uint8_t level) noexcept;
    void setBrightnessDefault(uint8_t level) noexcept;

    AppEeprom ep;
    AppFileSystem fs;
    AppCanvas cs;
    AppAudio audio;
    AppTouch touch;
    AppSleep sleep;
    MsgBox msgBox;

    SysVar<BkCmd> bkcmd;
    SysVar<uint32_t> sys[3];
    SysVar<uint8_t> pio[8];

protected:
    [[nodiscard]] virtual bool dispatchResponse(const Message& m, bool txIdle) noexcept;
    virtual void dispatchEvent(const Message& m) noexcept;

private:
    friend class SmartApp;
    friend class MsgBox;
    friend class Page;

    friend void nexComponentRegisterFailed(Application& app, Page& page, const Component& c, MISC::RegStatus st) noexcept;

    void registerPage(Page& page) noexcept;
    void abortSessionFault() noexcept;

    void dispatchError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;
    void dispatchSysVarResponse(uint8_t tag, const msg::getNumeric& response) noexcept;

    ScreenLayout _screen{};
    BIF::IByteStream& _stream;
    Gateway _gateway;
    Session _session;

    msg::Status _lastError{};
    uint8_t _lastErrorPage = 0u;
    uint8_t _lastErrorComp = 0u;

    MISC::ObjStorage<Page, kMaxPages, uint8_t, 0> _pageStorage; //TODO Сделал изменяемым
    uint8_t _currentPageId = 0xFF;
    ClockMsFn _clockMsFn;
    uint32_t _timeoutMs = kDefaultTimeoutMs;
};

} // namespace nex
