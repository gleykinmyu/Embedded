#pragma once

#include <cstdint>
#include <cstdio>
#include "../../Interfaces/ibyte_stream.hpp"
#include "nexComponentBase.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexErrors.hpp"
#include "../core/nexSession.hpp"
#include "nexApplicationFacades.hpp"

namespace nex {

// Слотов в таблице страниц; PageBase::ID < kMaxPages.
static constexpr unsigned kMaxPages = 16u;

// Таймаут ответа get/status после полного TX кадра; та же шкала, что now_ms в update.
static constexpr uint32_t kDefaultGetResponseTimeoutMs = 500u;

// Nextion-приложение: UART-шлюз, страницы, маршрутизация RX, фасады TX. Очередь и активная сессия — Session.
class Application 
{
public:
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

    [[nodiscard]] const ScreenLayout& screenLayout() const noexcept { return _screen; }

    // Поставить транзакцию в очередь; UART — в update(now_ms) или после завершения сессии.
    // TODO(QueueFull): при переполнении — отложить tx (буфер ожидания), по session timeout / completeTransaction
    // повторить tryEnqueue, чтобы команда не терялась после EnqueueRejected.
    void enqueue(Transaction tx) noexcept;

    // Опрос UART: TX, RX, таймаут ответа по now_ms (монотонные мс). true — был прогресс TX или принят кадр.
    bool update(uint32_t now_ms) noexcept;

    /** Согласовать с HMI `bkcmd`: при `false` панель не присылает завершающий status по командам —
     * транзакции `AwaitingStatus` после полного TX завершаются успешно без ожидания и без таймаута.
     * TODO: держать в синхроне с фактическим значением на дисплее (системная переменная `bkcmd` / чтение после загрузки HMI). */
    void setBkCmd(bool enabled) noexcept { _bkcmdEnabled = enabled; }
    /** TODO: при синхронизации с панелью отражать реальное `bkcmd`, а не только зеркало MCU. */
    [[nodiscard]] bool bkcmdEnabled() const noexcept { return _bkcmdEnabled; }

    /** Разрешить onError для политики `QueuePolicy::NotifyOptional` (NotIdle, QueueEmpty, …). По умолчанию `false`. */
    void setNotifyOptional(bool enabled) noexcept { _notifyOptional = enabled; }
    [[nodiscard]] bool notifyOptional() const noexcept { return _notifyOptional; }

    [[nodiscard]] Status lastStatus() const noexcept { return _lastStatus; }
    [[nodiscard]] NexErrors errors() const noexcept;
    void clearErrors() noexcept;

    [[nodiscard]] uint8_t currentPageId() const noexcept { return _currentPageId; }
    [[nodiscard]] Page* page(uint8_t id) noexcept;
    [[nodiscard]] const Page* page(uint8_t id) const noexcept;

    // nullptr — нет страницы или компонента comp_id.
    [[nodiscard]] Component* getComponent(uint8_t page_id, uint8_t comp_id) noexcept;


    // Обработчики событий: touch, touchXY, pageChange, systemEvent, transparentEvent, error; у страницы — onExit/onLoad при смене id.
    virtual void onTouch(const msg::evTouch&) {}
    virtual void onTouchXY(const msg::evTouchXY&) {}
    virtual void onPageChange(uint8_t) {}
    virtual void onSystemEvent(const msg::evSystem&) {}
    virtual void onTransparentEvent(const msg::evTransparent&) {}
    // status с панели (NIS) или синтетика MCU; page_id/comp_id == 0 — только глобальный onError (nexApplicationAddons.cpp).
    virtual void onError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;

    // --- доп. команды и UI (nexApplicationAddons.cpp) ---
    void calibrateTouch() noexcept;
    void restart() noexcept;
    void setRandGen(int32_t minVal, int32_t maxVal) noexcept;
    void play(uint32_t channel, uint32_t resourceId, uint32_t loop01) noexcept;
    void showErrorBox(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;

    AppEeprom ep;
    AppFileSystem fs;
    AppCanvas cs;

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

    // onTouch приложения, затем `PageBase::onTouch` зарегистрированной страницы (из dispatchEvent).
    void dispatchTouch(const msg::evTouch& e) noexcept;

    // `_currentPageId`, onPageChange, PageBase::onExit / onLoad при смене id (из dispatchEvent).
    void dispatchPageChange(const msg::evPage& e) noexcept;

    // onError приложения; при ненулевой паре page/comp — PageBase::onError. (0,0) — без страницы.
    void dispatchError(const msg::Status& status, uint8_t page_id = 0u, uint8_t comp_id = 0u) noexcept;

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

    static constexpr std::size_t kErrorBoxTextCap = 80u;

    ScreenLayout _screen{};
    char _error_box_text[kErrorBoxTextCap]{};
    BIF::IByteStream& _stream;
    Gateway _gateway;
    Session _session;
    Page* _pages[kMaxPages]{};
    uint8_t _currentPageId = 0xFF;
    /** Зеркало политики `bkcmd` на панели; TODO: синхронизировать с системной переменной дисплея. */
    bool _bkcmdEnabled = false;
    bool _notifyOptional = false;
    /** После `showErrorBox`: touch/touchXY → `restart()`, не в обработчики. */
    bool _touchBlocked = false;
    Status _lastStatus = Status::OK;
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
