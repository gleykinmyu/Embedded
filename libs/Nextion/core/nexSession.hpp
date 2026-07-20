#pragma once

#include <cstddef>
#include <cstdint>
#include "../nexConfig.hpp"
#include "nexCommands.hpp"
#include "nexTimeout.hpp"
#include "nexMessages.hpp"
#include "nexStatusMask.hpp"
#include "nexTypes.hpp"

/**
 * `Transaction` — метаданные tx и указатель на `Command` в слоте очереди.
 * `Session` — очередь, активная tx, `begin` / `transmit` / `pollTimeout` / `end`.
 */

namespace nex {

class Gateway;

struct Transaction {
    /** Вид активной tx: команда, `get` число/строка, transparent (R301). */
    enum class Kind : uint8_t {
        Command,
        GetNumeric,
        GetString,
        TransparentTx,
        RawDataRx,
    };

    Kind kind = Kind::Command;
    /** Маршрут для `onStatus` / `onResponse` этой tx. */
    /** Маршрут для `onStatus` / `onResponse` этой tx. */
    Route route;
    /** Атрибут виджета (`attr::Id`) или `SysVarTag` для зеркал и корреляции `get`. */
    uint8_t tag = 0u;
    /** Panel status, принимаемые как ответ этой tx; `msg::kAwaitingNone` = NoAwaiting. */
    msg::Status::Mask awaiting_status = msg::kAwaitingAllPanel;

    Transaction() noexcept = default;

    Transaction(const Command& cmd, Route route, uint8_t tag = 0u, Kind kind = Kind::Command,
        msg::Status::Mask awaiting_status = msg::kAwaitingAllPanel) noexcept;

    Transaction(const Command& cmd, uint8_t page, uint8_t comp, uint8_t tag = 0u,
        Kind kind = Kind::Command,
        msg::Status::Mask awaiting_status = msg::kAwaitingAllPanel) noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;

    [[nodiscard]] const Command& command() const noexcept;

    [[nodiscard]] bool emplace(void* storage, std::size_t maxBytes, std::size_t maxAlign) noexcept;

    /** Маска, по которой session ждёт panel-status после TX (NIS §6.13, `pollTimeout`).
     *  Не совпадает с `routeMask`: Off/OnFailure → `msg::kAwaitingNone` (Complete на txIdle). */
    [[nodiscard]] msg::Status::Mask sessionWaitMask(BkCmd bkcmd) const noexcept;

    /** Маска plausibility для route status к активной tx: `awaiting_status` × `msg::bkcmdAllowedStatus`.
     *  Может включать fail-биты при OnFailure, пока session ещё Active (до txIdle). */
    [[nodiscard]] bool correlatesWith(const msg::Status& msg, BkCmd bkcmd) const noexcept;

private:
    const Command* _command = nullptr;
};

namespace detail {

class TransactionQueue {
public:
    enum class Status : uint8_t {
        OK = 0,
        Full,
        EmptyTransaction,
        EmplaceFailed,
    };

    static const std::size_t kCapacity = static_cast<std::size_t>(NEX_SESSION_QUEUE_CAPACITY);
    static constexpr std::size_t kMaxObjectSize = 128u; //TODO перейти на плотную упаковку очереди
    static constexpr std::size_t kMaxAlign = alignof(std::max_align_t);

    TransactionQueue() noexcept = default;
    ~TransactionQueue() noexcept;

    TransactionQueue(const TransactionQueue&) = delete;
    TransactionQueue& operator=(const TransactionQueue&) = delete;

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::OK; }

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool isFull() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    [[nodiscard]] bool push(Transaction tr) noexcept;
    [[nodiscard]] const Transaction* peek() const noexcept;
    /** `index` от головы (0 = head); `nullptr` если `index >= count()`. */
    [[nodiscard]] const Transaction* at(std::size_t index) const noexcept;
    void pop() noexcept;
    void clear() noexcept;

private:
    static_assert(kCapacity > 0u, "NEX_SESSION_QUEUE_CAPACITY must be > 0");
    static_assert(kMaxObjectSize >= kMaxAlign);
    static_assert(kMaxObjectSize % kMaxAlign == 0u);
    static_assert(kMaxAlign >= alignof(std::max_align_t));

    struct Slot {
        alignas(kMaxAlign) unsigned char storage[kMaxObjectSize] = {};
        Transaction data = {};
    };

    Slot slots_[kCapacity] = {};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;
    Status _status = Status::OK;
};

} // namespace detail

/**
 * Очередь `Transaction` и жизненный цикл активной tx: `begin` → `transmit` → `pollTimeout` → `end`.
 * Вызывается из `Application::update()`; сбои → `AppError::Session` через `onStatus`.
 */
class Session {
public:
    using Kind = Transaction::Kind;

    enum class Status : uint8_t {
        Idle = 0,
        Active,               /**< begin: транзакция активирована, ожидание TX/ответа. */
        QueueFull,
        QueueEmptyTransaction,
        QueueEmplaceFailed,
        PushFailed,           /**< begin: `Gateway::pushCommand` провалился. */
        TransmitFailed,       /**< transmit: UART/шлюз не принял байты. */
        ReceiveFailed,        /**< RX: ошибка Gateway / IByteStream после приёма. */
        Timeout,              /**< нет ответа в срок. */
        Complete,     /**< TX готов, status с панели не ожидается — `end(true)` снаружи. */
    };

    Session() noexcept = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /** Одна попытка поставить tx в очередь; без `update()` / spin. */
    [[nodiscard]] bool tryEnqueue(Transaction tx) noexcept;

    /**
     * Активировать голову очереди: `pushCommand` в Gateway.
     * @return false — `PushFailed` или предусловия не выполнены (`Idle`, очередь пуста, TX занят).
     */
    [[nodiscard]] bool begin(Gateway& gateway) noexcept;

    /**
     * Продолжить TX активной транзакции (`Gateway::transmit`).
     * @return false — `TransmitFailed`; при `!isActive()` — true (нечего слать).
     */
    [[nodiscard]] bool transmit(Gateway& gateway, uint32_t now_ms, uint32_t timeout_ms) noexcept;

    /** Завершить активную транзакцию: снять таймер, pop головы очереди. @a success — сбросить статусы ошибок. */
    void end(bool success) noexcept;

    /** Get / Command с ожиданием panel-status; не assign (`kAwaitingNone`), не transparent. */
    [[nodiscard]] bool canRetryActive(BkCmd bkcmd) const noexcept;

    /** Повторная отправка active tx без `pop()`; вызывать при `canRetryActive` и `gateway.isTxIdle()`. */
    bool retryActive(Gateway& gateway, BkCmd bkcmd) noexcept;

    void resetActive() noexcept;
    void clearTimeout() noexcept;
    void noteReceiveFailed() noexcept { _status = Status::ReceiveFailed; }

    /**
     * Таймаут ответа: проверка дедлайна; после полного TX — постановка таймера или `ResponseComplete`.
     * @return true — смотреть `getStatus()` (`ResponseTimedOut` / `ResponseComplete`), завершить сессию снаружи.
     */
    [[nodiscard]] bool pollTimeout(Gateway& gateway, uint32_t now_ms, uint32_t timeout_ms, BkCmd bkcmd) noexcept;
    
    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept {
        _status = Status::Idle;
        _queue.clearError();
    }

    /** Отказ постановки после stall / без spin — не гасит TX (`isActive` на `_txActive`). */
    void noteQueueFull() noexcept { _status = Status::QueueFull; }

    /** TX в полёте (`begin`…`end`); не зависит от `_status` (может быть `QueueFull` параллельно). */
    [[nodiscard]] inline bool isActive() const noexcept { return _txActive; }
    [[nodiscard]] inline bool hasQueued() const noexcept { return !_queue.isEmpty(); }
    [[nodiscard]] inline bool isQueueFull() const noexcept { return _queue.isFull(); }
    [[nodiscard]] inline std::size_t queuedCount() const noexcept { return _queue.count(); }
    [[nodiscard]] inline const Transaction* current() const noexcept { return _queue.peek(); }
    /** `index` от головы очереди (0 = current); `nullptr` если вне диапазона. */
    [[nodiscard]] inline const Transaction* queuedAt(std::size_t index) const noexcept {
        return _queue.at(index);
    }

private:
    detail::TransactionQueue _queue;
    Status _status = Status::Idle;
    bool _txActive = false;

    MsTimer _responseTimer{};
};

inline const char* cstr(Session::Status s) noexcept {
    switch (s) {
    case Session::Status::Idle: return "Idle";
    case Session::Status::Active: return "Active";
    case Session::Status::QueueFull: return "QueueFull";
    case Session::Status::QueueEmptyTransaction: return "QueueEmptyTransaction";
    case Session::Status::QueueEmplaceFailed: return "QueueEmplaceFailed";
    case Session::Status::PushFailed: return "PushFailed";
    case Session::Status::TransmitFailed: return "TransmitFailed";
    case Session::Status::ReceiveFailed: return "ReceiveFailed";
    case Session::Status::Timeout: return "TimedOut";
    case Session::Status::Complete: return "Complete";
    default: return "?";
    }
}

} // namespace nex
