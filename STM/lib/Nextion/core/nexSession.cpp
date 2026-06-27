#include "nexSession.hpp"
#include "nexGateway.hpp"

namespace nex {
namespace detail {

namespace {

constexpr std::size_t nextSlotIndex(std::size_t index, std::size_t capacity) noexcept {
    return (index + 1u) % capacity;
}

} // namespace

TransactionQueue::~TransactionQueue() noexcept {
    clear();
}

bool TransactionQueue::isEmpty() const noexcept {
    return count_ == 0u;
}

bool TransactionQueue::isFull() const noexcept {
    return count_ == kCapacity;
}

bool TransactionQueue::push(Transaction tr) noexcept {
    if (isFull()) {
        _status = Status::Full;
        return false;
    }
    if (tr.isEmpty()) {
        _status = Status::EmptyTransaction;
        return false;
    }

    Slot& s = slots_[tail_];
    if (!tr.emplace(s.storage, kMaxObjectSize, kMaxAlign)) {
        _status = Status::EmplaceFailed;
        return false;
    }
    s.data = tr;
    tail_ = nextSlotIndex(tail_, kCapacity);
    ++count_;
    clearError();
    return true;
}

const Transaction* TransactionQueue::peek() const noexcept {
    if (isEmpty())
        return nullptr;
    return &slots_[head_].data;
}

void TransactionQueue::pop() noexcept {
    if (isEmpty())
        return;
    Slot& s = slots_[head_];
    s.data.command().destroyIn(s.storage);
    s.data = {};
    head_ = nextSlotIndex(head_, kCapacity);
    --count_;
}

void TransactionQueue::clear() noexcept {
    while (!isEmpty())
        pop();
}

} // namespace detail

Transaction::Transaction(const Command& cmd, Route route, uint8_t tag, Kind kind,
    msg::Status::Mask awaiting_status) noexcept
    : kind(kind)
    , route(route)
    , tag(tag)
    , awaiting_status(awaiting_status)
    , _command(&cmd)
{}

Transaction::Transaction(const Command& cmd, uint8_t page, uint8_t comp, uint8_t tag, Kind kind,
    msg::Status::Mask awaiting_status) noexcept
    : Transaction(cmd, Route{page, comp}, tag, kind, awaiting_status)
{}

bool Transaction::isEmpty() const noexcept {
    return _command == nullptr;
}

const Command& Transaction::command() const noexcept {
    if (_command == nullptr)
        return kEmptyCommand();
    return *_command;
}

bool Transaction::emplace(void* storage, std::size_t maxBytes, std::size_t maxAlign) noexcept {
    if (isEmpty())
        return false;
    if (!_command->emplaceIn(storage, maxBytes, maxAlign))
        return false;
    _command = static_cast<const Command*>(storage);
    return true;
}

msg::Status::Mask Transaction::sessionWaitMask(BkCmd bkcmd) const noexcept {
    // Блокировка очереди: что ждём в pollTimeout после txIdle (не correlate).
    switch (bkcmd) {
    case BkCmd::Off:
    case BkCmd::OnFailure:
        return msg::kAwaitingNone;
    case BkCmd::OnSuccess:
        return awaiting_status & msg::kAwaitingSuccessOnly;
    case BkCmd::Always:
        return awaiting_status;
    }
    return msg::kAwaitingNone;
}

namespace {

Session::Status queueToSession(detail::TransactionQueue::Status st) noexcept {
    switch (st) {
    case detail::TransactionQueue::Status::Full:
        return Session::Status::QueueFull;
    case detail::TransactionQueue::Status::EmptyTransaction:
        return Session::Status::QueueEmptyTransaction;
    case detail::TransactionQueue::Status::EmplaceFailed:
        return Session::Status::QueueEmplaceFailed;
    default:
        return Session::Status::Idle;
    }
}

} // namespace

bool Session::tryEnqueue(Transaction tx) noexcept {
    if (_queue.push(tx)) {
        if (_status != Status::Active)
            _status = Status::Idle;
        return true;
    }
    _status = queueToSession(_queue.getStatus());
    return false;
}

bool Session::begin(Gateway& gateway) noexcept {
    if (!hasQueued() || isActive() || !gateway.isTxIdle())
        return false;

    const Transaction* const queued = _queue.peek();
    clearError();

    if (!gateway.pushCommand(queued->command())) {
        _status = Status::PushFailed;
        return false;
    }

    clearTimeout();
    _status = Status::Active;
    return true;
}

bool Session::transmit(Gateway& gateway, uint32_t now_ms, uint32_t timeout_ms) noexcept {
    if (!isActive())
        return true;

    if (!gateway.transmit(now_ms, timeout_ms)) {
        _status = Status::TransmitFailed;
        return false;
    }
    return true;
}

void Session::end(bool success) noexcept {
    clearTimeout();
    _queue.pop();
    if (success)
        clearError();
}

bool Session::canRetryActive(BkCmd bkcmd) const noexcept {
    if (!isActive())
        return false;

    const Transaction* const tx = current();
    if (tx == nullptr || tx->isEmpty())
        return false;

    switch (tx->kind) {
    case Transaction::Kind::GetNumeric:
    case Transaction::Kind::GetString:
        return true;
    case Transaction::Kind::Command:
        return tx->sessionWaitMask(bkcmd) != msg::kAwaitingNone;
    case Transaction::Kind::TransparentTx:
    case Transaction::Kind::RawDataRx:
        return false;//TODO доделать когда вернемся к TransparentTx и RawDataRx
    }
    return false;
}

bool Session::retryActive(Gateway& gateway, BkCmd bkcmd) noexcept {
    if (!canRetryActive(bkcmd))
        return false;

    if (!gateway.isTxIdle())
        return false;

    const Transaction* const tx = current();
    if (tx == nullptr || tx->isEmpty())
        return false;

    if (!gateway.pushCommand(tx->command())) {
        _status = Status::PushFailed;
        return false;
    }

    clearTimeout();
    _status = Status::Active;
    return true;
}

bool Session::pollTimeout(Gateway& gateway, uint32_t now_ms, uint32_t timeout_ms, BkCmd bkcmd) noexcept {
    if (_responseTimer.isRunning() && isActive()) {
        if (_responseTimer.timedOut(now_ms)) {
            _status = Status::Timeout;
            return true;
        }
    }

    if (!gateway.isTxIdle() || !isActive())
        return false;

    const Transaction* const tx = current();
    if (tx == nullptr)
        return false;

    switch (tx->kind) {
    case Transaction::Kind::TransparentTx:
    case Transaction::Kind::RawDataRx:
        return false;

    case Transaction::Kind::Command:
        if (tx->sessionWaitMask(bkcmd) == msg::kAwaitingNone) {
            _status = Status::Complete;
            return true;
        }
        break;

    case Transaction::Kind::GetNumeric:
    case Transaction::Kind::GetString:
        break;
    }

    _responseTimer.startOnce(now_ms, timeout_ms);
    return false;
}

void Session::clearTimeout() noexcept {
    _responseTimer.stop();
}

void Session::resetActive() noexcept {
    clearTimeout();
    if (_status == Status::Active)
        _status = Status::Idle;
}

bool Transaction::correlatesWith(const msg::Status& msg, BkCmd bkcmd) const noexcept {
    using Code = msg::Status::Code;
    if (msg.status == Code::AppError 
        || msg.status == Code::Unrecognized_Header
        || msg.status == Code::Serial_Overflow)
        return false;

    return msg.codeInMask(awaiting_status & msg::bkcmdAllowedStatus(bkcmd));
}

} // namespace nex
