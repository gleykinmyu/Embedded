#pragma once

#include <cstdint>
#include <cstdio>
#include "../../Interfaces/ibyte_stream.hpp"
#include "../../Interfaces/obj_registry.hpp"
#include "../comp/nexComponentBase.hpp"
#include "../core/nexTypes.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexDebug.hpp"
#include "../core/nexSession.hpp"
#include "nexApplicationFacades.hpp"
#include "../comp/nexMsgBox.hpp"
#include "nexSysVars.hpp"
#include "nexCompIdMap.hpp"
#include "error/nexAppErrorHandler.hpp"

namespace nex {

// Слотов в таблице страниц; PageBase::ID < kMaxPages.
static constexpr unsigned kMaxPages = 16u;

// Таймаут ответа get/status после полного TX кадра; та же шкала, что now_ms в update.
static constexpr uint32_t kDefaultGetResponseTimeoutMs = 500u;

// Nextion-приложение: UART-шлюз, страницы, маршрутизация RX, фасады TX. Очередь и активная сессия — Session.
class Application 
{
public:
    /** Метаданные транзакции для `SysVar`; см. `Route::kSysVarPageId` / `kSysVarCompId`. */
    static constexpr uint8_t kSysVarRoutePageId = Route::kSysVarPageId;
    static constexpr uint8_t kSysVarRouteCompId = Route::kSysVarCompId;

    /** Исход MCU в `msg::Status::tag_1` при `AppError` (alias `AppStatus`). */
    using Status = AppStatus;

    explicit Application(BIF::IByteStream& stream, uint16_t screen_width, uint16_t screen_height,
        CompIdMapTable& id_map_table) noexcept;
    virtual ~Application() = default;

    // Поставить транзакцию в очередь; UART — в update(now_ms) или после завершения сессии.
    // TODO(QueueFull): при переполнении — отложить tx (буфер ожидания), по session timeout / completeTransaction
    // повторить tryEnqueue, чтобы команда не терялась после EnqueueRejected.
    void enqueue(Transaction tx) noexcept;

    // Опрос UART: TX, RX, таймаут ответа по now_ms (монотонные мс). true — был прогресс TX или принят кадр.
    bool update(uint32_t now_ms) noexcept;

    /** Разрешить onError для политики `QueuePolicy::NotifyOptional` (NotIdle, QueueEmpty, …). По умолчанию `false`. */
    void setNotifyOptional(bool enabled) noexcept { _errors.setNotifyOptional(enabled); }
    [[nodiscard]] bool notifyOptional() const noexcept { return _errors.notifyOptional(); }

    [[nodiscard]] Status lastStatus() const noexcept { return _errors.lastStatus(); }
    [[nodiscard]] const msg::Status& lastError() const noexcept { return _errors.lastError(); }
    [[nodiscard]] uint8_t lastErrorPage() const noexcept { return _errors.lastErrorPage(); }
    [[nodiscard]] uint8_t lastErrorComp() const noexcept { return _errors.lastErrorComp(); }
    [[nodiscard]] NexErrors errors() const noexcept;
    void clearErrors() noexcept;

    /** UART отключён (`Disconnected`); `enqueue`/`update` TX приостановлены до восстановления. */
    [[nodiscard]] bool isLinkDown() const noexcept { return _errors.isLinkDown(); }




    // Обработчики событий: touch, touchXY, pageChange, systemEvent, transparentEvent, error; у страницы — onExit/onLoad при смене id.
    virtual void onTouch(const msg::evTouch&) {}
    virtual void onTouchXY(const msg::evTouchXY&) {}
    /** `evMsgBox`: только при маршруте `(0, 0)`; иначе — `Page` / `Component`, заданные в `MsgBox::setRoute` / `showError`. */
    virtual void onMsgBox(const msg::evMsgBox& e) noexcept { (void)e; }
    virtual void onPageChange(uint8_t) {}
    virtual void onSystemEvent(const msg::evSystem&) {}
    virtual void onTransparentEvent(const msg::evTransparent&) {}
    /** Ответ `get` по системной переменной (`tag` — `SysVar::tag`); разбор и `applyResponse` — в наследнике. */
    virtual void onSysResponse(uint8_t tag, const msg::getNumeric& response) noexcept { (void)tag; (void)response; }
    /**
     * Завершён опрос `.id` в режиме `Discover` (`idMap.setMode(Discover)` до первого `update`).
     * При `success` режим уже `Flash`; таблица — буфер, переданный в конструктор `Application`.
     */
    virtual void onCompIdMapComplete(bool success) noexcept { (void)success; }

    // status с панели (NIS) или синтетика MCU; page_id/comp_id == 0 — только глобальный onError (nexApplicationAddons.cpp).
    virtual void onError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;

    // --- доп. команды и UI (nexApplicationAddons.cpp) ---
    void restartScreen() noexcept;
    /** Переключить страницу на панели (`page N`). */
    void switchPage(uint8_t pageId) noexcept;
    /** Запросить id текущей страницы (`sendme`); ответ — `msg::evPage`. */
    void requestCurrentPage() noexcept;
    /** Перерисовать текущую страницу (`ref 0`). */
    void refreshPage() noexcept;

    [[nodiscard]] uint8_t currentPageId() const noexcept { return _currentPageId; }
    
    // nullptr — нет страницы или компонента comp_id.
    [[nodiscard]] Page* getPage(uint8_t id) noexcept;
    /** Число зарегистрированных страниц (`0..pageCount()-1` подряд). */
    [[nodiscard]] uint8_t pageCount() const noexcept {
        return _pageStorage.registeredCount();
    }
    [[nodiscard]] Component* getComponent(uint8_t page_id, uint8_t comp_id) noexcept;

    [[nodiscard]] const ScreenLayout& screenLayout() const noexcept { return _screen; }

    /** Текущая скорость UART (`baud`) / сохранённая при power-on (`bauds`), NIS §6. */
    void setBaudrate(Baudrate rate) noexcept;
    void setBaudrateDefault(Baudrate rate) noexcept;
    /** Адрес панели в режиме Address Mode (`addr`, 0…2815). */
    void setAddress(uint16_t address) noexcept;
    /** Яркость подсветки (`dim`, 0…100). */
    void setBrightness(uint8_t level) noexcept;
    /** Яркость при power-on (`dims`, 0…100). */
    void setBrightnessDefault(uint8_t level) noexcept;

    AppEeprom ep;
    AppFileSystem fs;
    AppCanvas cs;
    AppAudio audio;
    AppTouch touch;
    AppSleep sleep;
    MsgBox msgBox;
    /** Карта panel `.id` ↔ objname (Compiled / Discover / Flash). */
    CompIdMap idMap;

    // --- системные переменные NIS (§6) ---
    SysVar<BkCmd> bkcmd;
    SysVar<uint32_t> sys[3];
    SysVar<uint8_t> pio[8];

private:
    friend class AppErrorHandler;
    friend class CompIdMap;
    friend class MsgBox;
    friend class Page;

    
    friend void nexComponentRegisterFailed(Application& app, Page& page, const Component& c, MISC::RegStatus st) noexcept;

    // Регистрация страницы в `_pageStorage` (из ctor `Page`). При ошибке — `onError` (`PageRegisterFailed`).
    void registerPage(Page& page) noexcept;

    void reportRegisterError(Status appStatus, MISC::RegStatus code, uint8_t page_id, uint8_t comp_id) noexcept {
        _errors.reportRegisterError(appStatus, code, page_id, comp_id);
    }

    // При idle-сессии и свободном TX взять голову очереди, `pushCommand`. false — activate/push провал;
    // после успешного push — `_session.clearTimeout()` под новый дедлайн ответа.
    void sessionBegin() noexcept;

    // Завершение активной UART-транзакции: `_session.completeTransaction` (снять таймер, снять голову очереди).
    // Success — зарезервировано для наследников; по умолчанию true.
    void sessionEnd(bool Success = true) noexcept;

    // Шаг TX: при необходимости `sessionBegin`, затем `Gateway::transmit`.
    // true — нечего слать, байт ушёл или write==0 при OK потока (отложить).
    // false — ошибка UART при TX: `AppErrorHandler::handle` (stream/gateway failure).
    [[nodiscard]] bool sessionTransmit() noexcept;

    // Просроченный дедлайн ожидания ответа в Session: SessionTimeout, `dispatchError`, `sessionEnd(false)`.
    void checkSessionTimeout(uint32_t now_ms) noexcept;

    // --- dispatch (входящие кадры / ошибки) ---
    // Ответ активной UART-транзакции при полном TX (`txIdle`). true — кадр поглощён, не в dispatchEvent.
    [[nodiscard]] bool dispatchResponse(const Message& m, bool txIdle) noexcept;

    // Фоновое сообщение: не ответ текущей транзакции (после dispatchResponse с false).
    void dispatchEvent(const Message& m) noexcept;

    // `evTouch` / `evTouchXY` в `m`: onTouch(+Page) или onTouchXY; при активном `msgBox` — `msgBox.onTouchXY` (click = release на той же кнопке).
    void dispatchTouch(const Message& m) noexcept;

    // `evMsgBox` — к `Page` / `Component` по `page_id` / `comp_id`, иначе `onMsgBox` приложения.
    void dispatchMsgBox(const msg::evMsgBox& e) noexcept;

    // `_currentPageId`, onPageChange, PageBase::onExit / onLoad при смене id (из dispatchEvent).
    void dispatchPageChange(const msg::evPage& e) noexcept;

    void dispatchError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept {
        _errors.dispatchError(status, page_id, comp_id);
    }

    void dispatchSysVarResponse(uint8_t tag, const msg::getNumeric& response) noexcept;

protected:
    /** Переопределение политики; `QueuePolicy::None` — `defaultQueuePolicy(failure.cause)`. */
    [[nodiscard]] virtual QueuePolicy queuePolicy(const AppFailure& failure) const noexcept;
    [[nodiscard]] virtual uint8_t resetActiveRetryLimit() const noexcept;

private:
    ScreenLayout _screen{};
    BIF::IByteStream& _stream;
    Gateway _gateway;
    Session _session;
    AppErrorHandler _errors;
    
    MISC::ObjStorage<Page, kMaxPages, uint8_t, 0> _pageStorage;
    uint8_t _currentPageId = 0xFF;
};

inline std::size_t formatStatusMessage(const msg::Status& st, uint8_t page_id, uint8_t comp_id, char* buf,
    std::size_t cap) noexcept {
    if (cap == 0u)
        return 0u;
    if (st.status == msg::Status::Code::AppError) {
        const auto appSt = static_cast<AppStatus>(st.tag_1);
        const ErrorDetail detail = unpackDetail(st.tag_2);
        if (st.tag_2 != 0u) {
            return static_cast<std::size_t>(std::snprintf(buf, cap, "AppError %s %s p%u c%u",
                appStatusCstr(appSt), errorDetailCstr(detail), static_cast<unsigned>(page_id),
                static_cast<unsigned>(comp_id)));
        }
        return static_cast<std::size_t>(std::snprintf(buf, cap, "AppError %s p%u c%u", appStatusCstr(appSt),
            static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id)));
    }
    return static_cast<std::size_t>(std::snprintf(buf, cap, "%s t1=%u t2=%u p%u c%u", statusCodeCstr(st.status),
        static_cast<unsigned>(st.tag_1), static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id),
        static_cast<unsigned>(comp_id)));
}

inline void printStatusError(const msg::Status& st, uint8_t page_id, uint8_t comp_id) noexcept {
#if defined(NEX_DEBUG)
    if (st.status == msg::Status::Code::AppError) {
        const auto appSt = static_cast<AppStatus>(st.tag_1);
        if (st.tag_2 != 0u) {
            const ErrorDetail detail = unpackDetail(st.tag_2);
            NEX_DBG("[Nextion] onError AppError %s %s page=%u comp=%u\n", appStatusCstr(appSt),
                errorDetailCstr(detail), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        } else {
            NEX_DBG("[Nextion] onError AppError %s page=%u comp=%u\n", appStatusCstr(appSt),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        }
        return;
    }
    NEX_DBG("[Nextion] onError %s (0x%02X) tag_1=%u tag_2=%u page=%u comp=%u\n", statusCodeCstr(st.status),
        static_cast<unsigned>(static_cast<uint8_t>(st.status)), static_cast<unsigned>(st.tag_1),
        static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
#else
    (void)st;
    (void)page_id;
    (void)comp_id;
#endif
}

} // namespace nex
