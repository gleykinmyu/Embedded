#pragma once

#include <cstdint>
#include <cstdio>
#include "../../Interfaces/ibyte_stream.hpp"
#include "nexComponentBase.hpp"
#include "../core/nexTypes.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexErrors.hpp"
#include "../core/nexSession.hpp"
#include "nexApplicationFacades.hpp"
#include "nexMsgBox.hpp"
#include "nexSysVars.hpp"

namespace nex {

// Слотов в таблице страниц; PageBase::ID < kMaxPages.
static constexpr unsigned kMaxPages = 16u;

// Таймаут ответа get/status после полного TX кадра; та же шкала, что now_ms в update.
static constexpr uint32_t kDefaultGetResponseTimeoutMs = 500u;

// Nextion-приложение: UART-шлюз, страницы, маршрутизация RX, фасады TX. Очередь и активная сессия — Session.
class Application 
{
public:
    /** Метаданные транзакции для `SysVar` (вне таблицы страниц / компонентов). */
    static constexpr uint8_t kSysVarRoutePageId = 0xFFu;
    static constexpr uint8_t kSysVarRouteCompId = 0xFFu;

    /** Исход MCU в `msg::Status::tag_1` при `AppError`. */
    enum class Status : uint8_t {
        OK = 0,
        GatewayPushFailed,
        GatewayTransmitFailed,
        GatewayReceiveFailed,
        EnqueueRejected,
        SessionActivateFailed,
        SessionTimeout,
        PageRegisterFailed,       /**< `registerPage`: id ≥ kMaxPages или слот занят другим объектом. */
        ComponentRegisterFailed,  /**< `Page::registerComponent` / `rebindComponentId`. */
    };

    explicit Application(BIF::IByteStream& stream, uint16_t panel_width, uint16_t panel_height) noexcept;
    virtual ~Application() = default;

    // Поставить транзакцию в очередь; UART — в update(now_ms) или после завершения сессии.
    // TODO(QueueFull): при переполнении — отложить tx (буфер ожидания), по session timeout / completeTransaction
    // повторить tryEnqueue, чтобы команда не терялась после EnqueueRejected.
    void enqueue(Transaction tx) noexcept;

    // Опрос UART: TX, RX, таймаут ответа по now_ms (монотонные мс). true — был прогресс TX или принят кадр.
    bool update(uint32_t now_ms) noexcept;

    /** Разрешить onError для политики `QueuePolicy::NotifyOptional` (NotIdle, QueueEmpty, …). По умолчанию `false`. */
    void setNotifyOptional(bool enabled) noexcept { _notifyOptional = enabled; }
    [[nodiscard]] bool notifyOptional() const noexcept { return _notifyOptional; }

    [[nodiscard]] Status lastStatus() const noexcept { return _lastStatus; }
    [[nodiscard]] const msg::Status& lastError() const noexcept { return _lastError; }
    [[nodiscard]] uint8_t lastErrorPage() const noexcept { return _lastErrorPage; }
    [[nodiscard]] uint8_t lastErrorComp() const noexcept { return _lastErrorComp; }
    [[nodiscard]] NexErrors errors() const noexcept;
    void clearErrors() noexcept;

    [[nodiscard]] uint8_t currentPageId() const noexcept { return _currentPageId; }
    [[nodiscard]] Page* page(uint8_t id) noexcept;
    [[nodiscard]] const Page* page(uint8_t id) const noexcept;

    // nullptr — нет страницы или компонента comp_id.
    [[nodiscard]] Component* getComponent(uint8_t page_id, uint8_t comp_id) noexcept;
    [[nodiscard]] const ScreenLayout& screenLayout() const noexcept { return _screen; }


    // Обработчики событий: touch, touchXY, pageChange, systemEvent, transparentEvent, error; у страницы — onExit/onLoad при смене id.
    virtual void onTouch(const msg::evTouch&) {}
    virtual void onTouchXY(const msg::evTouchXY&) {}
    virtual void onPageChange(uint8_t) {}
    virtual void onSystemEvent(const msg::evSystem&) {}
    virtual void onTransparentEvent(const msg::evTransparent&) {}
    /** Ответ `get` по системной переменной (`tag` — `SysVar::tag`); разбор и `applyResponse` — в наследнике. */
    virtual void onSysResponse(uint8_t tag, const msg::getNumeric& response) noexcept { (void)tag; (void)response; }
    // status с панели (NIS) или синтетика MCU; page_id/comp_id == 0 — только глобальный onError (nexApplicationAddons.cpp).
    virtual void onError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;


    /** @deprecated используйте `msgBox.show` / `msgBox.showError`. */
    void showErrorBox(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept {
        _lastError = status;
        _lastErrorPage = page_id;
        _lastErrorComp = comp_id;
        msgBox.showError();
    }

    // --- доп. команды и UI (nexApplicationAddons.cpp) ---
    void restart() noexcept;
    /** Переключить страницу на панели (`page N`). */
    void switchPage(uint8_t pageId) noexcept;
    /** Запросить id текущей страницы (`sendme`); ответ — `msg::evPage`. */
    void requestCurrentPage() noexcept;
    /** Перерисовать текущую страницу (`ref 0`). */
    void refreshPage() noexcept;

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

    // --- системные переменные NIS (§6) ---
    SysVar<BkCmd> bkcmd;
    SysVar<uint32_t> sys[3];
    SysVar<uint8_t> pio[8];

private:
    friend class Page;
    friend void nexComponentRegisterFailed(Application& app, Page& page, const Component* c,
        unsigned maxComponents) noexcept;
    friend void nexComponentRegistryFull(Application& app, Page& page, unsigned maxComponents) noexcept;

    // Регистрация страницы в `_pages` (из ctor `Page`). При ошибке — `onError` (`PageRegisterFailed`).
    void registerPage(Page& page) noexcept;

    /** Сбой `registerPage` / `registerComponent`: пишет `lastStatus`, вызывает `onError` с `AppError` и `RegisterError` в `tag_2`. */
    void reportRegisterError(Status appStatus, RegisterError code, uint8_t page_id, uint8_t comp_id) noexcept;

    // При idle-сессии и свободном TX взять голову очереди, `pushCommand`. false — activate/push провал;
    // после успешного push — `_session.clearTimeout()` под новый дедлайн ответа.
    void sessionBegin() noexcept;

    // Завершение активной UART-транзакции: `_session.completeTransaction` (снять таймер, снять голову очереди).
    // Success — зарезервировано для наследников; по умолчанию true.
    void sessionEnd(bool Success = true) noexcept;

    // Шаг TX: при необходимости `sessionBegin`, затем `Gateway::transmit`.
    // true — нечего слать, байт ушёл или write==0 при OK потока (отложить).
    // false — ошибка UART при TX: handleGatewayStreamFailure (onError + recovery очереди).
    [[nodiscard]] bool sessionTransmit() noexcept;

    // Просроченный дедлайн ожидания ответа в Session: SessionTimeout, `dispatchError`, `sessionEnd(false)`.
    void checkSessionTimeout(uint32_t now_ms) noexcept;

    // --- dispatch (входящие кадры / ошибки) ---
    // Ответ активной UART-транзакции при полном TX (`txIdle`). true — кадр поглощён, не в dispatchEvent.
    [[nodiscard]] bool dispatchResponse(const Message& m, bool txIdle) noexcept;

    // Фоновое сообщение: не ответ текущей транзакции (после dispatchResponse с false).
    void dispatchEvent(const Message& m) noexcept;

    // `evTouch` / `evTouchXY` в `m`: onTouch(+Page) или onTouchXY; при активном `msgBox` — только `msgBox.onTouchXY`.
    void dispatchTouch(const Message& m) noexcept;

    // `_currentPageId`, onPageChange, PageBase::onExit / onLoad при смене id (из dispatchEvent).
    void dispatchPageChange(const msg::evPage& e) noexcept;

    // onError приложения; при ненулевой паре page/comp — PageBase::onError. (0,0) — без страницы.
    void dispatchError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;

    void dispatchSysVarResponse(uint8_t tag, const msg::getNumeric& response) noexcept;

    // --- ошибки MCU и политики очереди (nexApplicationErrors.cpp) ---
    virtual QueuePolicy enqueueFailurePolicy(Session::Status sessionStatus) const noexcept;
    virtual QueuePolicy gatewayPushFailurePolicy(Gateway::Status gatewayStatus) const noexcept;
    virtual QueuePolicy sessionActivateFailurePolicy(Session::Status sessionStatus) const noexcept;

    void handleEnqueueFailure(const Transaction& tx) noexcept;
    void handleGatewayPushFailure() noexcept;
    void handleSessionActivateFailure() noexcept;
    void handleGatewayStreamFailure(Status appStatus) noexcept;
    void notifyAppError(QueuePolicy policy, Status appStatus, ErrorDetail detail, uint8_t page_id,
        uint8_t comp_id) noexcept;
    void applyQueueRecovery(QueuePolicy policy) noexcept;

    enum class FailureRecovery : uint8_t { None, IfSessionActive, Always };
    void routeFromActive(uint8_t& page_id, uint8_t& comp_id) const noexcept;
    void finalizeAppFailure(Status appStatus, QueuePolicy policy, ErrorDetail detail, uint8_t page_id,
        uint8_t comp_id, FailureRecovery recovery) noexcept;
    void clearGatewayStream() noexcept;

    ScreenLayout _screen{};
    BIF::IByteStream& _stream;
    Gateway _gateway;
    Session _session;
    Page* _pages[kMaxPages]{};
    uint8_t _currentPageId = 0xFF;
    bool _notifyOptional = false;
    Status _lastStatus = Status::OK;
    msg::Status _lastError{};
    uint8_t _lastErrorPage = 0u;
    uint8_t _lastErrorComp = 0u;
};

inline const char* applicationStatusCstr(Application::Status s) noexcept {
    switch (s) {
    case Application::Status::OK: return "OK";
    case Application::Status::GatewayPushFailed: return "GatewayPushFailed";
    case Application::Status::GatewayTransmitFailed: return "GatewayTransmitFailed";
    case Application::Status::GatewayReceiveFailed: return "GatewayReceiveFailed";
    case Application::Status::EnqueueRejected: return "EnqueueRejected";
    case Application::Status::SessionActivateFailed: return "SessionActivateFailed";
    case Application::Status::SessionTimeout: return "SessionTimeout";
    case Application::Status::PageRegisterFailed: return "PageRegisterFailed";
    case Application::Status::ComponentRegisterFailed: return "ComponentRegisterFailed";
    default: return "?";
    }
}

inline msg::Status makeAppError(Application::Status appStatus, ErrorDetail detail = {}) noexcept {
    msg::Status st{};
    st.status = msg::Status::Code::AppError;
    st.tag_1 = static_cast<uint8_t>(appStatus);
    st.tag_2 = (detail.subsystem != ErrorSubsystem::None) ? packDetail(detail.subsystem, detail.code) : 0u;
    return st;
}

inline std::size_t formatStatusMessage(const msg::Status& st, uint8_t page_id, uint8_t comp_id, char* buf,
    std::size_t cap) noexcept {
    if (cap == 0u)
        return 0u;
    if (st.status == msg::Status::Code::AppError) {
        const auto appSt = static_cast<Application::Status>(st.tag_1);
        const ErrorDetail detail = unpackDetail(st.tag_2);
        if (st.tag_2 != 0u) {
            return static_cast<std::size_t>(std::snprintf(buf, cap, "AppError %s %s p%u c%u",
                applicationStatusCstr(appSt), errorDetailCstr(detail), static_cast<unsigned>(page_id),
                static_cast<unsigned>(comp_id)));
        }
        return static_cast<std::size_t>(std::snprintf(buf, cap, "AppError %s p%u c%u", applicationStatusCstr(appSt),
            static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id)));
    }
    return static_cast<std::size_t>(std::snprintf(buf, cap, "%s t1=%u t2=%u p%u c%u", statusCodeCstr(st.status),
        static_cast<unsigned>(st.tag_1), static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id),
        static_cast<unsigned>(comp_id)));
}

inline void printStatusError(const msg::Status& st, uint8_t page_id, uint8_t comp_id) noexcept {
    if (st.status == msg::Status::Code::AppError) {
        const auto appSt = static_cast<Application::Status>(st.tag_1);
        if (st.tag_2 != 0u) {
            const ErrorDetail detail = unpackDetail(st.tag_2);
            std::printf("[Nextion] onError AppError %s %s page=%u comp=%u\n", applicationStatusCstr(appSt),
                errorDetailCstr(detail), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        } else {
            std::printf("[Nextion] onError AppError %s page=%u comp=%u\n", applicationStatusCstr(appSt),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        }
        return;
    }
    std::printf("[Nextion] onError %s (0x%02X) tag_1=%u tag_2=%u page=%u comp=%u\n", statusCodeCstr(st.status),
        static_cast<unsigned>(static_cast<uint8_t>(st.status)), static_cast<unsigned>(st.tag_1),
        static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
}

} // namespace nex
