#include "nexSession.hpp"

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
        return Session::Status::OK;
    }
}

} // namespace

bool Session::tryEnqueue(Transaction tx) noexcept {
    if (_queue.push(tx)) {
        if (_status != Status::NotIdle)
            _status = Status::OK;
        return true;
    }
    _status = queueToSession(_queue.getStatus());
    return false;
}

bool Session::activateHead() noexcept {
    // Не ошибка приложения: сессия уже ведёт UART-транзакцию (sessionBegin обычно отсекает раньше).
    if (!_active.isIdle()) {
        _status = Status::NotIdle;
        return false;
    }
    const Transaction* const head = _queue.peek();
    // Не ошибка: рассинхрон с hasQueued() или очередь опустела между проверками.
    if (head == nullptr) {
        _status = Status::QueueEmpty;
        return false;
    }
    _active = *head;
    clearError();
    return true;
}

void Session::clearTimeout() noexcept {
    _responseDeadlineActive = false;
    _responseDeadlineMs = 0u;
}

void Session::resetActive() noexcept {
    clearTimeout();
    _active = {};
}

void Session::startResponseTimeout(uint32_t now_ms, uint32_t timeout_ms) noexcept {
    if (!_active.isResponse() || _responseDeadlineActive)
        return;

    _responseDeadlineMs = now_ms + timeout_ms;
    _responseDeadlineActive = true;
}

bool Session::isResponseTimedOut(uint32_t now_ms) const noexcept {
    if (!_responseDeadlineActive || isIdle())
        return false;

    constexpr uint32_t kMsHalfModulus = UINT32_C(1) << 31;
    return (now_ms - _responseDeadlineMs) < kMsHalfModulus;
}

void Session::completeTransaction() noexcept {
    clearTimeout();
    _active = {};
    _queue.pop();
}

} // namespace nex
