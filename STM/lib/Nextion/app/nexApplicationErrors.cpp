// nexApplicationErrors.cpp — MCU-ошибки, политики очереди и handle*Failure для Application.

#include "nexApplication.hpp"
#include "../core/nexErrors.hpp"

namespace nex {

void Application::reportRegisterError(Status appStatus, RegisterError code, uint8_t page_id,
    uint8_t comp_id) noexcept {
    notifyAppError(QueuePolicy::Notify, appStatus, detailFrom(code), page_id, comp_id);
}

NexErrors Application::errors() const noexcept {
    NexErrors e{};
    e.stream = _stream.getStatus();
    e.gateway = _gateway.getStatus();
    e.session = _session.getStatus();
    return e;
}

void Application::clearErrors() noexcept {
    _lastStatus = Status::OK;
    _stream.clearErrors();
    _gateway.clearError();
    _session.clearError();
}

QueuePolicy Application::enqueueFailurePolicy(Session::Status sessionStatus) const noexcept {
    return defaultQueuePolicy(sessionStatus);
}

QueuePolicy Application::gatewayPushFailurePolicy(Gateway::Status gatewayStatus) const noexcept {
    return defaultQueuePolicy(gatewayStatus);
}

QueuePolicy Application::sessionActivateFailurePolicy(Session::Status sessionStatus) const noexcept {
    return defaultQueuePolicy(sessionStatus);
}

void Application::notifyAppError(QueuePolicy policy, Status appStatus, ErrorDetail detail, uint8_t page_id,
    uint8_t comp_id) noexcept {
    _lastStatus = appStatus;
    switch (policy) {
    case QueuePolicy::Notify:
    case QueuePolicy::DropHead:
    case QueuePolicy::ResetActive:
        dispatchError(makeAppError(appStatus, detail), page_id, comp_id);
        break;
    case QueuePolicy::NotifyOptional:
        if (_notifyOptional)
            dispatchError(makeAppError(appStatus, detail), page_id, comp_id);
        break;
    default:
        break;
    }
}

// TODO(ResetActiveRetry): голова остаётся; повтор в sessionBegin при isTxIdle (не плотный цикл в одном update).
// При персистентном сбое (TX/RX, TxBusy) — onError каждый update без эскалации. Лимит попыток на голову
// или DropHead / отдельный Status после N неудач.
// TODO(TransmitRetryLimit): лимит повторов передачи (ResetActive / re-push при OverFlowTX). Связано с
// TODO(ByteStreamPolicy) в nexErrors.hpp.
void Application::applyQueueRecovery(QueuePolicy policy) noexcept {
    if (policy == QueuePolicy::DropHead)
        _session.completeTransaction();
    else if (policy == QueuePolicy::ResetActive)
        _session.resetActive();
}

void Application::routeFromActive(uint8_t& page_id, uint8_t& comp_id) const noexcept {
    if (_session.isIdle())
        return;
    const Transaction& active = _session.active();
    page_id = active.page_id;
    comp_id = active.comp_id;
}

void Application::finalizeAppFailure(Status appStatus, QueuePolicy policy, ErrorDetail detail, uint8_t page_id,
    uint8_t comp_id, FailureRecovery recovery) noexcept {
    notifyAppError(policy, appStatus, detail, page_id, comp_id);
    switch (recovery) {
    case FailureRecovery::Always:
        applyQueueRecovery(policy);
        break;
    case FailureRecovery::IfSessionActive:
        if (!_session.isIdle())
            applyQueueRecovery(policy);
        break;
    case FailureRecovery::None:
    default:
        break;
    }
}

void Application::clearGatewayStream() noexcept {
    _stream.clearErrors();
    _gateway.clearError();
}

// tryEnqueue вернул false: транзакция в очередь не попала (исходная команда ещё у вызывающего, не в слоте).
void Application::handleEnqueueFailure(const Transaction& tx) noexcept {
    // 1) Базовая причина — Session (QueueFull, QueueEmptyTransaction, QueueEmplaceFailed, …).
    // TODO(QueueFull): не отбрасывать сразу — держать tx до освобождения места (таймаут сессии / sessionEnd),
    // затем снова tryEnqueue; сейчас при QueueFull транзакция пропадает для вызывающего.
    const Session::Status err = _session.getStatus();
    QueuePolicy policy = enqueueFailurePolicy(err);
    ErrorDetail detail = detailFrom(err);
    // 2) QueueEmplaceFailed: emplaceIn уже выставил Command::Status (SlotTooSmall / SlotAlignMismatch).
    if (err == Session::Status::QueueEmplaceFailed) {
        const Command::Status cmdSt = tx.command().getStatus();
        detail = detailFrom(cmdSt);
        policy = defaultQueuePolicy(cmdSt);
    }
    // 3) onError по политике; DropHead здесь не снимает очередь — отклонённая tx в неё не попала.
    finalizeAppFailure(Status::EnqueueRejected, policy, detail, tx.page_id, tx.comp_id, FailureRecovery::None);
    if (!tx.isEmpty())
        tx.command().clearErrors();
    _session.clearError();
}

// sessionBegin: после успешного activateHead — active не idle; pushCommand в TX framer не удался.
void Application::handleGatewayPushFailure() noexcept {
    const Gateway::Status gw = _gateway.getStatus();
    const Command::Status cmdSt = _session.active().command().getStatus();
    // SerializeFailed: приоритет детали Command; иначе политика/деталь по Gateway (TxBusy → ResetActive, …).
    QueuePolicy policy{};
    ErrorDetail detail{};
    if (gw == Gateway::Status::SerializeFailed) {
        policy = defaultQueuePolicy(cmdSt);
        detail = detailFrom(cmdSt);
    } else {
        policy = gatewayPushFailurePolicy(gw);
        detail = detailFrom(gw);
    }
    uint8_t page_id = 0u;
    uint8_t comp_id = 0u;
    routeFromActive(page_id, comp_id);
    finalizeAppFailure(Status::GatewayPushFailed, policy, detail, page_id, comp_id, FailureRecovery::Always);
    _gateway.clearError();
}

// activateHead: NotIdle / QueueEmpty → NotifyOptional (onError по `notifyOptional()`).
void Application::handleSessionActivateFailure() noexcept {
    const Session::Status err = _session.getStatus();
    finalizeAppFailure(Status::SessionActivateFailed, sessionActivateFailurePolicy(err), detailFrom(err), 0u, 0u,
        FailureRecovery::None);
    _session.clearError();
}

// transmit(): tick сбросил framer; при активной сессии — recovery, иначе голова зависнет без re-push.
// Конец update: после receive stream или gateway всё ещё в ошибке (RX overflow, read fail, …).
// TODO(ByteStreamPolicy): Disconnected — halt Application (см. nexErrors.hpp); BitError/FrameError — baud/reinit.
void Application::handleGatewayStreamFailure(Status appStatus) noexcept {
    const Gateway::Status gw = _gateway.getStatus();
    const BIF::IByteStream::Status streamSt = _stream.getStatus();
    uint8_t page_id = 0u;
    uint8_t comp_id = 0u;
    routeFromActive(page_id, comp_id);
    QueuePolicy policy = QueuePolicy::Notify;
    ErrorDetail detail{};
    if (streamSt != BIF::IByteStream::Status::OK) {
        policy = defaultQueuePolicy(streamSt);
        detail = detailFrom(streamSt);
    } else if (gw != Gateway::Status::OK) {
        policy = defaultQueuePolicy(gw);
        detail = detailFrom(gw);
    }
    finalizeAppFailure(appStatus, policy, detail, page_id, comp_id, FailureRecovery::IfSessionActive);
    clearGatewayStream();
}

} // namespace nex
