#include "nexAppErrorHandler.hpp"
#include "../nexApplication.hpp"
#include "../../core/nexGateway.hpp"

namespace nex {

namespace {

QueuePolicy policyForStream(BIF::IByteStream::Status st) noexcept {
    switch (st) {
    case BIF::IByteStream::Status::OK:
        return QueuePolicy::None;
    case BIF::IByteStream::Status::OverFlowTX:
    case BIF::IByteStream::Status::OverFlowRX:
        return QueuePolicy::ResetActive;
    case BIF::IByteStream::Status::Disconnected:
    case BIF::IByteStream::Status::BitError:
    case BIF::IByteStream::Status::FrameError:
        return QueuePolicy::DropHead;
    default:
        return QueuePolicy::Notify;
    }
}

QueuePolicy policyForGateway(Gateway::Status s) noexcept {
    switch (s) {
    case Gateway::Status::TxBusy:
    case Gateway::Status::TxBusyRaw:
    case Gateway::Status::StreamTxError:
        return QueuePolicy::ResetActive;
    case Gateway::Status::SerializeFailed:
    case Gateway::Status::EmptyPayload:
    case Gateway::Status::NullPointer:
        return QueuePolicy::DropHead;
    default:
        return QueuePolicy::Notify;
    }
}

QueuePolicy policyForSession(Session::Status st) noexcept {
    switch (st) {
    case Session::Status::NotIdle:
    case Session::Status::QueueEmpty:
        return QueuePolicy::NotifyOptional;
    default:
        return QueuePolicy::Notify;
    }
}

QueuePolicy policyForCommand(Command::Status st) noexcept {
    switch (st) {
    case Command::Status::NullPointer:
    case Command::Status::EmptyLiteral:
    case Command::Status::EmptyAttribute:
    case Command::Status::InvalidColor:
    case Command::Status::InvalidGeometry:
    case Command::Status::InvalidFields:
    case Command::Status::UnknownKind:
    case Command::Status::SlotTooSmall:
    case Command::Status::SlotAlignMismatch:
        return QueuePolicy::DropHead;
    default:
        return QueuePolicy::Notify;
    }
}

void routeFromActive(const Session& session, FailureRoute& route) noexcept {
    if (session.isIdle())
        return;
    const Transaction& active = session.active();
    route.page_id = active.page_id;
    route.comp_id = active.comp_id;
}

enum class RecoveryMode : uint8_t { None, IfSessionActive, Always };

RecoveryMode recoveryModeFor(AppOperation op) noexcept {
    switch (op) {
    case AppOperation::GatewayPushFailed:
        return RecoveryMode::Always;
    case AppOperation::GatewayTransmitFailed:
    case AppOperation::GatewayReceiveFailed:
        return RecoveryMode::IfSessionActive;
    default:
        return RecoveryMode::None;
    }
}

bool shouldRunRecovery(RecoveryMode mode, bool sessionActive) noexcept {
    switch (mode) {
    case RecoveryMode::Always:
        return true;
    case RecoveryMode::IfSessionActive:
        return sessionActive;
    default:
        return false;
    }
}

void clearLayerErrors(AppOperation op, Session& session, BIF::IByteStream& stream, Gateway& gateway) noexcept {
    switch (op) {
    case AppOperation::EnqueueRejected:
    case AppOperation::SessionActivateFailed:
        session.clearError();
        break;
    case AppOperation::GatewayPushFailed:
        gateway.clearError();
        break;
    case AppOperation::GatewayTransmitFailed:
    case AppOperation::GatewayReceiveFailed:
        stream.clearErrors();
        gateway.clearError();
        break;
    default:
        break;
    }
}

} // namespace

QueuePolicy defaultQueuePolicy(ErrorDetail cause) noexcept {
    switch (cause.reporter) {
    case ErrorReporter::Stream:
        return policyForStream(static_cast<BIF::IByteStream::Status>(cause.code));
    case ErrorReporter::Gateway:
        return policyForGateway(static_cast<Gateway::Status>(cause.code));
    case ErrorReporter::Session:
        return policyForSession(static_cast<Session::Status>(cause.code));
    case ErrorReporter::Command:
        return policyForCommand(static_cast<Command::Status>(cause.code));
    case ErrorReporter::Domain:
        return QueuePolicy::Notify;
    default:
        return QueuePolicy::Notify;
    }
}

AppFailure captureEnqueueFailure(const Transaction& tx, Session& session) noexcept {
    AppFailure f{};
    f.operation = AppOperation::EnqueueRejected;
    f.route = {tx.page_id, tx.comp_id};
    f.related_tx = &tx;

    const Session::Status err = session.getStatus();
    f.cause = detailFrom(err);
    if (err == Session::Status::QueueEmplaceFailed)
        f.cause = detailFrom(tx.command().getStatus());
    return f;
}

AppFailure captureSessionActivateFailure(Session& session) noexcept {
    AppFailure f{};
    f.operation = AppOperation::SessionActivateFailed;
    f.cause = detailFrom(session.getStatus());
    return f;
}

AppFailure captureGatewayPushFailure(Session& session, Gateway& gateway) noexcept {
    AppFailure f{};
    f.operation = AppOperation::GatewayPushFailed;
    routeFromActive(session, f.route);

    if (gateway.getStatus() == Gateway::Status::SerializeFailed)
        f.cause = detailFrom(session.active().command().getStatus());
    else
        f.cause = detailFrom(gateway.getStatus());
    return f;
}

AppFailure captureGatewayStreamFailure(AppOperation operation, Session& session, BIF::IByteStream& stream,
    Gateway& gateway) noexcept {
    AppFailure f{};
    f.operation = operation;
    routeFromActive(session, f.route);

    if (stream.getStatus() != BIF::IByteStream::Status::OK)
        f.cause = detailFrom(stream.getStatus());
    else
        f.cause = detailFrom(gateway.getStatus());
    return f;
}

AppFailure makeRegisterFailure(AppOperation operation, MISC::RegStatus code, uint8_t page_id,
    uint8_t comp_id) noexcept {
    AppFailure f{};
    f.operation = operation;
    f.cause = detailFrom(code);
    f.route = {page_id, comp_id};
    return f;
}

AppFailure makeSessionTimeout(const Transaction& active) noexcept {
    AppFailure f{};
    f.operation = AppOperation::SessionTimeout;
    f.route = {active.page_id, active.comp_id};
    return f;
}

AppErrorHandler::AppErrorHandler(Application& app, BIF::IByteStream& stream, Gateway& gateway,
    Session& session) noexcept
    : _app(app)
    , _stream(stream)
    , _gateway(gateway)
    , _session(session) {}

void AppErrorHandler::clearErrors() noexcept {
    _lastStatus = AppStatus::OK;
    _lastError = msg::Status{};
    _lastErrorPage = 0u;
    _lastErrorComp = 0u;
    _headRecoveryAttempts = 0u;
    _linkDown = false;
}

void AppErrorHandler::clearLinkDownIfStreamOk() noexcept {
    if (_linkDown && _stream.getStatus() == BIF::IByteStream::Status::OK)
        _linkDown = false;
}

QueuePolicy AppErrorHandler::resolvePolicy(const AppFailure& failure) const noexcept {
    const QueuePolicy override = _app.queuePolicy(failure);
    if (override != QueuePolicy::None)
        return override;
    return defaultQueuePolicy(failure.cause);
}

void AppErrorHandler::handle(AppFailure failure) noexcept {
    const QueuePolicy policy = resolvePolicy(failure);

    if (failure.cause.reporter == ErrorReporter::Stream
        && static_cast<BIF::IByteStream::Status>(failure.cause.code) == BIF::IByteStream::Status::Disconnected) {
        _linkDown = true;
    }

    _lastStatus = static_cast<AppStatus>(failure.operation);
    const uint8_t page_id = failure.route.page_id;
    const uint8_t comp_id = failure.route.comp_id;

    switch (policy) {
    case QueuePolicy::Notify:
    case QueuePolicy::DropHead:
    case QueuePolicy::ResetActive:
        dispatchError(toWire(failure), page_id, comp_id);
        break;
    case QueuePolicy::NotifyOptional:
        if (_notifyOptional)
            dispatchError(toWire(failure), page_id, comp_id);
        break;
    default:
        break;
    }

    const RecoveryMode recovery = recoveryModeFor(failure.operation);
    if (shouldRunRecovery(recovery, !_session.isIdle())) {
        if (policy == QueuePolicy::ResetActive) {
            ++_headRecoveryAttempts;
            if (_headRecoveryAttempts >= _app.resetActiveRetryLimit()) {
                _headRecoveryAttempts = 0u;
                _session.completeTransaction();
            } else {
                _session.resetActive();
            }
        } else if (policy == QueuePolicy::DropHead) {
            _headRecoveryAttempts = 0u;
            _session.completeTransaction();
        }
    }

    clearLayerErrors(failure.operation, _session, _stream, _gateway);
    if (failure.related_tx != nullptr && !failure.related_tx->isEmpty())
        failure.related_tx->command().clearErrors();
}

void AppErrorHandler::dispatchError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept {
    _lastError = status;
    _lastErrorPage = page_id;
    _lastErrorComp = comp_id;
    if (status.status == msg::Status::Code::AppError)
        _lastStatus = static_cast<AppStatus>(status.tag_1);
    _app.onError(status, page_id, comp_id);
    if (page_id == 0u && comp_id == 0u)
        return;
    if (Page* const p = _app.getPage(page_id))
        p->onError(status, comp_id);
}

} // namespace nex
