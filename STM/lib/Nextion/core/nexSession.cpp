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

Transaction::Transaction(const Command& cmd, uint8_t page_id, uint8_t comp_id, uint8_t tag, Kind kind,
    AwaitingStatus awaiting_status) noexcept
    : kind(kind)
    , page_id(page_id)
    , comp_id(comp_id)
    , tag(tag)
    , awaiting_status(awaiting_status)
    , _command(&cmd)
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

AwaitingStatus Transaction::sessionWaitMask(BkCmd bkcmd) const noexcept {
    switch (bkcmd) {
    case BkCmd::Off:
    case BkCmd::OnFailure:
        return kAwaitingNone;
    case BkCmd::OnSuccess:
        return awaiting_status & kAwaitingSuccessOnly;
    case BkCmd::Always:
        return awaiting_status;
    }
    return kAwaitingNone;
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

const Transaction* Session::faultingTransaction() const noexcept {
    if (!_active.isEmpty())
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

    switch (_active.kind) {
    case Transaction::Kind::TransparentTx:
    case Transaction::Kind::RawDataRx:
        return false;

    case Transaction::Kind::Command:
        if (_active.sessionWaitMask(bkcmd) == kAwaitingNone) {
            _status = Status::Complete;
            return true;
        }
        break;

    case Transaction::Kind::GetNumeric:
    case Transaction::Kind::GetString:
        break;
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

AwaitingStatus Transaction::statusCorrelateMask(BkCmd bkcmd) const noexcept {
    return awaiting_status & bkcmdAllowedStatus(bkcmd);
}

bool Transaction::statusCorrelatesWithTransaction(const msg::Status& status, BkCmd bkcmd) const noexcept {
    if (status.status == msg::Status::Code::AppError
        || status.status == msg::Status::Code::Unrecognized_Header)
        return false;
    if (isBkcmdIndependentStatus(status.status))
        return false;
    return statusInMask(status.status, statusCorrelateMask(bkcmd));
}

} // namespace nex
