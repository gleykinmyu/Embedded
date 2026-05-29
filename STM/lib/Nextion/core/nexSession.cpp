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

const Transaction* Session::faultingTransaction() const noexcept {
    if (!_active.isIdle())
        return &_active;
    return _queue.peek();
}

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
    if (!hasQueued() || !isIdle() || !gateway.isTxIdle())
        return false;

    const Transaction* const queued = _queue.peek();
    clearError();

    if (!gateway.pushCommand(queued->command())) {
        _status = Status::PushFailed;
        return false;
    }

    _active = *queued;
    clearTimeout();
    _status = Status::Active;
    return true;
}

bool Session::transmit(Gateway& gateway) noexcept {
    if (isIdle())
        return true;

    if (!gateway.transmit()) {
        _status = Status::TransmitFailed;
        return false;
    }
    return true;
}

void Session::end(bool success) noexcept {
    clearTimeout();
    _active = {};
    _queue.pop();
    if (success)
        clearError();
}

bool Session::pollTimeout(Gateway& gateway, uint32_t now_ms, uint32_t timeout_ms, BkCmd bkcmd) noexcept {
    if (_responseDeadlineActive && !isIdle()) {
        constexpr uint32_t kMsHalfModulus = UINT32_C(1) << 31;
        if ((now_ms - _responseDeadlineMs) < kMsHalfModulus) {
            _status = Status::Timeout;
            return true;
        }
    }

    if (!gateway.isTxIdle() || isIdle())
        return false;

    switch (_active.state) {
    case Transaction::State::AwaitingTransparentTx:
    case Transaction::State::AwaitingRawDataRx:
        return false;

    case Transaction::State::AwaitingStatus:
        if (bkcmd != BkCmd::Always) {
            _status = Status::Complete;
            return true;
        }
        break;

    case Transaction::State::AwaitingNumericGet:
    case Transaction::State::AwaitingStringGet:
        break;

    case Transaction::State::Idle:
        return false;
    }

    if (!_responseDeadlineActive) {
        _responseDeadlineMs = now_ms + timeout_ms;
        _responseDeadlineActive = true;
    }
    return false;
}

void Session::clearTimeout() noexcept {
    _responseDeadlineActive = false;
    _responseDeadlineMs = 0u;
}

void Session::resetActive() noexcept {
    clearTimeout();
    _active = {};
    if (_status == Status::Active)
        _status = Status::Idle;
}

} // namespace nex
