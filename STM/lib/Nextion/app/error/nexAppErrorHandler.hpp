#pragma once

#include "nexErrors.hpp"
#include "../../core/nexSession.hpp"
#include <cstdint>

namespace nex {

class Application;
class Gateway;

struct FailureRoute {
    uint8_t page_id = 0u;
    uint8_t comp_id = 0u;
};

/** Снимок для `handle()`: сценарий, деталь, маршрут. Recovery/clear слоёв — внутри handler по `operation`. */
struct AppFailure {
    AppOperation operation = AppOperation::OK;
    ErrorDetail cause{};
    FailureRoute route{};
    const Transaction* related_tx = nullptr;
};

[[nodiscard]] inline msg::Status toWire(const AppFailure& failure) noexcept {
    return makeAppError(static_cast<AppStatus>(failure.operation), failure.cause);
}

AppFailure captureEnqueueFailure(const Transaction& tx, Session& session) noexcept;
AppFailure captureSessionActivateFailure(Session& session) noexcept;
AppFailure captureGatewayPushFailure(Session& session, Gateway& gateway) noexcept;
AppFailure captureGatewayStreamFailure(AppOperation operation, Session& session, BIF::IByteStream& stream,
    Gateway& gateway) noexcept;
AppFailure makeRegisterFailure(AppOperation operation, MISC::RegStatus code, uint8_t page_id,
    uint8_t comp_id) noexcept;
AppFailure makeSessionTimeout(const Transaction& active) noexcept;

class AppErrorHandler {
public:
    AppErrorHandler(Application& app, BIF::IByteStream& stream, Gateway& gateway, Session& session) noexcept;

    void setNotifyOptional(bool enable) noexcept { _notifyOptional = enable; }
    [[nodiscard]] bool notifyOptional() const noexcept { return _notifyOptional; }

    void clearErrors() noexcept;
    [[nodiscard]] AppStatus lastStatus() const noexcept { return _lastStatus; }
    [[nodiscard]] const msg::Status& lastError() const noexcept { return _lastError; }
    [[nodiscard]] uint8_t lastErrorPage() const noexcept { return _lastErrorPage; }
    [[nodiscard]] uint8_t lastErrorComp() const noexcept { return _lastErrorComp; }

    [[nodiscard]] bool isLinkDown() const noexcept { return _linkDown; }
    void clearLinkDownIfStreamOk() noexcept;

    void handle(AppFailure failure) noexcept;

    void reportRegisterError(AppStatus status, MISC::RegStatus code, uint8_t page_id, uint8_t comp_id) noexcept {
        handle(makeRegisterFailure(static_cast<AppOperation>(status), code, page_id, comp_id));
    }

    void dispatchError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept;

    void onSessionEnded() noexcept { _headRecoveryAttempts = 0u; }

private:
    [[nodiscard]] QueuePolicy resolvePolicy(const AppFailure& failure) const noexcept;

    Application& _app;
    BIF::IByteStream& _stream;
    Gateway& _gateway;
    Session& _session;

    bool _notifyOptional = false;
    bool _linkDown = false;
    AppStatus _lastStatus = AppStatus::OK;
    msg::Status _lastError{};
    uint8_t _lastErrorPage = 0u;
    uint8_t _lastErrorComp = 0u;
    uint8_t _headRecoveryAttempts = 0u;
};

} // namespace nex
