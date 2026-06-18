#pragma once

/**
 * @file nexSession.hpp
 *
 * **Transaction** — метаданные транзакции и указатель на `Command` (в слоте очереди после `emplace`).
 * **Session** — очередь, активная транзакция, `begin`/`transmit`/`end` через `Gateway`.
 */

#include <cstddef>
#include <cstdint>
#include "nexCommands.hpp"
#include "nexMessages.hpp"
#include "nexStatusMask.hpp"
#include "nexTypes.hpp"

namespace nex {

class Gateway;

struct Transaction {
    enum class Kind : uint8_t {
        Command,
        GetNumeric,
        GetString,
        TransparentTx,
        RawDataRx,
    };

    Kind kind = Kind::Command;
    uint8_t page_id = 0u;
    uint8_t comp_id = 0u;
    uint8_t tag = 0u;
    /** Panel status (wire), принимаемые как ответ этой tx; `kAwaitingNone` = NoAwaiting. */
    AwaitingStatus awaiting_status = kAwaitingAllPanel;

    Transaction() noexcept = default;

    Transaction(const Command& cmd, uint8_t page_id, uint8_t comp_id, uint8_t tag = 0u,
        Kind kind = Kind::Command,
        AwaitingStatus awaiting_status = kAwaitingAllPanel) noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;

    [[nodiscard]] const Command& command() const noexcept;

    [[nodiscard]] bool emplace(void* storage, std::size_t maxBytes, std::size_t maxAlign) noexcept;

    /** Маска, по которой session ждёт panel-status после TX (NIS §6.13, pollTimeout). */
    [[nodiscard]] AwaitingStatus sessionWaitMask(BkCmd bkcmd) const noexcept;

    /** Маска correlate: tx × то, что панель может прислать при текущем bkcmd. */
    [[nodiscard]] AwaitingStatus statusCorrelateMask(BkCmd bkcmd) const noexcept;
    [[nodiscard]] bool statusCorrelatesWithTransaction(const msg::Status& msg, BkCmd bkcmd) const noexcept;

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

    static constexpr std::size_t kCapacity = 64u; //TODO сделать настраиваемым
    static constexpr std::size_t kMaxObjectSize = 128u; //TODO сделать настраиваемым
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
    void pop() noexcept;
    void clear() noexcept;

private:
    static_assert(kCapacity > 0u);
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

    [[nodiscard]] bool tryEnqueue(Transaction tx) noexcept;

    /**
     * Активировать голову очереди: `pushCommand` в Gateway.
     * @return false — `PushFailed` или предусловия не выполнены (`Idle`, очередь пуста, TX занят).
     */
    [[nodiscard]] bool begin(Gateway& gateway) noexcept;

    /**
     * Продолжить TX активной транзакции (`Gateway::transmit`).
     * @return false — `TransmitFailed`; при `isIdle()` — true (нечего слать).
     */
    [[nodiscard]] bool transmit(Gateway& gateway) noexcept;

    /** Завершить активную транзакцию: снять таймер, pop головы очереди. @a success — сбросить статусы ошибок. */
    void end(bool success) noexcept;

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

    [[nodiscard]] bool isIdle() const noexcept { return _active.isEmpty(); }
    [[nodiscard]] bool hasQueued() const noexcept { return !_queue.isEmpty(); }
    [[nodiscard]] std::size_t queuedCount() const noexcept { return _queue.count(); }

    [[nodiscard]] const Transaction& active() const noexcept { return _active; }

    /** `Active`, иначе голова очереди (например `PushFailed` до активации). */
    [[nodiscard]] const Transaction* faultingTransaction() const noexcept;

private:
    detail::TransactionQueue _queue;
    Transaction _active{};
    Status _status = Status::Idle;
    
    bool _responseDeadlineActive = false;
    uint32_t _responseDeadlineMs = 0u;
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
    case Session::Status::Timeout: return "ResponseTimedOut";
    case Session::Status::Complete: return "ResponseComplete";
    default: return "?";
    }
}

} // namespace nex
