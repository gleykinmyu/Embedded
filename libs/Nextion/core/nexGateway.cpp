#include "nexGateway.hpp"
#include "nexDebug.hpp"

namespace nex
{
//===============================================
// RxFramer
//===============================================

void RxFramer::reset() {
    _state = State::WaitHeader;
    _terms = 0;
    _overflowReportPending = false;
    frame.length = 0;
}

void RxFramer::enterResync(uint8_t byte) noexcept {
    _overflowReportPending = true;
    frame.length = 0;
    _state = State::Resync;
    _terms = (byte == Physical::TERM_BYTE) ? 1u : 0u;
}

bool RxFramer::getOverflowReport() noexcept {
    if (!_overflowReportPending)
        return false;
    _overflowReportPending = false;
    return true;
}

bool RxFramer::tryComplete() noexcept {
    if (_terms < Physical::TERM_COUNT || frame.length < Physical::TERM_COUNT)
        return false;
    const uint16_t n = static_cast<uint16_t>(frame.length - Physical::TERM_COUNT);
    if (!msg::frameLengthOk(frame.header, n))
        return false;
    frame.length = n;
    _terms = 0;
    _state = State::WaitHeader;
    return true;
}

bool RxFramer::appendByte(uint8_t byte) {
    switch (_state)
    {
    // NIS §18 — заголовок: первый байт после `0xFF×3`; лишние `0xFF` между кадрами пропускаем.
    case State::WaitHeader:
        if (byte == Physical::TERM_BYTE)
            return false;
        frame.header = byte;
        frame.length = 1;
        _terms = 0;
        _state = State::Collect;
        break;

    // NIS §17 — payload + скользящие `0xFF×3`; кадр только если длина совпала (кроме `0x70`).
    case State::Collect:
        if (msg::payloadSize(frame.length) >= RxFrame::MAX_PAYLOAD) {
            enterResync(byte);
            break;
        }
        frame.payload[msg::payloadSize(frame.length)] = byte;
        ++frame.length;
        if (byte == Physical::TERM_BYTE)
            ++_terms;
        else
            _terms = 0;
        return tryComplete();

    // После overflow: только поиск `0xFF×3`, кадр не собираем.
    case State::Resync:
        if (byte == Physical::TERM_BYTE) {
            if (++_terms >= Physical::TERM_COUNT) {
                _state = State::WaitHeader;
                _terms = 0;
            }
        } else {
            _terms = 0;
        }
        break;

    default:
        break;
    }
    return false;
}

//===============================================
// TranslateMessage
//===============================================

void TranslateMessage(const RxFrame& f, Message& out)
{
    const uint8_t h = f.header;

    // Длину кадра уже проверил `RxFramer`. `0x00`: status (1) или startup (3).
    if (h == 0u && f.length == msg::evSystem::StartupLength) {
        out = msg::evSystem{msg::evSystem::Code::StartupPreamble};
        return;
    }

    switch (h) {
    case msg::getString::Header: {
        msg::getString s{};
        s.length = msg::payloadSize(f.length);
        if (s.length > 0u)
            std::memcpy(s.chars, f.payload, s.length);
        if (s.length < RxFrame::MAX_PAYLOAD)
            s.chars[s.length] = '\0';
        out = s;
        return;
    }
    case msg::getNumeric::Header: {
        msg::getNumeric n{};
        std::memcpy(&n.value, f.payload, msg::payloadSize(msg::getNumeric::Length));
        out = n;
        return;
    }
    case msg::evTouch::Header:
        out = msg::evTouch{
            .route = Route{f.payload[0], f.payload[1]},
            .state = static_cast<TouchState>(f.payload[2]),
        };
        return;
    case static_cast<uint8_t>(msg::evTouchXY::Mode::Awake):
    case static_cast<uint8_t>(msg::evTouchXY::Mode::Sleep):
        out = msg::evTouchXY{
            .mode = static_cast<msg::evTouchXY::Mode>(h),
            .pos = Point{
                static_cast<Coord>((uint16_t(f.payload[0]) << 8) | f.payload[1]),
                static_cast<Coord>((uint16_t(f.payload[2]) << 8) | f.payload[3]),
            },
            .state = static_cast<TouchState>(f.payload[4]),
        };
        return;
    case msg::evPage::Header:
        out = msg::evPage{.page = f.payload[0]};
        return;
    case static_cast<uint8_t>(msg::evTransparent::Code::BlockComplete):
    case static_cast<uint8_t>(msg::evTransparent::Code::ReadyToReceive):
        out = msg::evTransparent{static_cast<msg::evTransparent::Code>(h)};
        return;
    case static_cast<uint8_t>(msg::evSystem::Code::AutoEnteredSleepMode):
    case static_cast<uint8_t>(msg::evSystem::Code::AutoWakeFromSleep):
    case static_cast<uint8_t>(msg::evSystem::Code::NextionReady):
    case static_cast<uint8_t>(msg::evSystem::Code::StartMicroSdUpgrade):
        out = msg::evSystem{static_cast<msg::evSystem::Code>(h)};
        return;
    default:
        if (h <= static_cast<uint8_t>(msg::Status::Code::Serial_Overflow))
            out = msg::Status{static_cast<msg::Status::Code>(h)};
        else
            out = msg::Status{msg::Status::Code::Unrecognized_Header};
        return;
    }
}

//===============================================
// TxFramer
//===============================================

void TxFramer::reset() noexcept {
    _state = State::Idle;
    frame.length = 0;
    _pos = 0;
    _rawData = nullptr;
    _rawLen = 0u;
}

bool TxFramer::isIdle() const noexcept {
    return _state == State::Idle && frame.length == 0u;
}

bool TxFramer::beginRaw(const uint8_t* data, size_t len) noexcept {
    if (!isIdle() || data == nullptr || len == 0u)
        return false;
    _rawData = data;
    _rawLen = len;
    _pos = 0;
    _state = State::RawPayload;
    return true;
}

TxFramer::Status TxFramer::tick(BIF::IByteStream& stream) noexcept {
    if (!stream.isOpen())
        return Status::Closed;

    if (_state == State::Idle && frame.length > 0u) {
        std::memcpy(frame.payload + frame.length, Physical::FRAME_TERMINATORS, Physical::TERM_COUNT);
        frame.length += Physical::TERM_COUNT;
        _state = State::FramePayload;
    }

    while (_state == State::FramePayload) {
        const size_t w = stream.write(frame.payload + _pos, static_cast<size_t>(frame.length - _pos));
        if (w == 0u)
            return Status::WaitTxSpace;
        _pos += w;
        if (_pos >= frame.length) {
            reset();
            return Status::OK;
        }
    }

    while (_state == State::RawPayload) {
        const size_t w = stream.write(_rawData + _pos, _rawLen - _pos);
        if (w == 0u)
            return Status::WaitTxSpace;
        _pos += w;
        if (_pos >= _rawLen) {
            reset();
            return Status::OK;
        }
    }
    return Status::OK;
}

//===============================================
// Gateway
//===============================================

Gateway::Gateway(BIF::IByteStream& s) : _stream(s) {}

void Gateway::recoverStreamRxOverFlow() noexcept {
    _rxFramer.reset();
    _stream.purge();
    _stream.clearErrors();
    _status = Status::StreamRxError;
}

void Gateway::onStreamReadFault(BIF::IByteStream::Status streamSt) noexcept {
    if (streamSt == BIF::IByteStream::Status::OverFlowRX) {
        recoverStreamRxOverFlow();
        return;
    }
    if (streamSt == BIF::IByteStream::Status::DataError) {
        _rxFramer.reset();
        _stream.purge();
        _status = Status::StreamRxError;
    }
}

bool Gateway::pushCommand(const Command& cmd) {
    if (!_txFramer.isIdle()) {
        _status = Status::TxBusy;
        return false;
    }

    if (!cmd.serialize(_txFramer.frame)) {
        _txFramer.reset();
        _status = Status::SerializeFailed; // деталь — `cmd.getStatus()`
        return false;
    }

    if (_txFramer.frame.length == 0u) {
        _txFramer.reset();
        _status = Status::EmptyPayload;
        return false;
    }

    misc::printTxPayloadLine("TX ", _txFramer.frame);

    clearError();
    return true;
}

bool Gateway::transmit(uint32_t now_ms, uint32_t timeout_ms) noexcept {
    if (_txFramer.isIdle()) {
        _txStallTimer.stop();
        return true;
    }

    const TxFramer::Status txSt = _txFramer.tick(_stream);
    switch (txSt) {
    case TxFramer::Status::Closed:
        _txFramer.reset();
        _stream.purgeOutput();
        _txStallTimer.stop();
        _status = Status::StreamTxError;
        return false;

    case TxFramer::Status::WaitTxSpace:
        _txStallTimer.startOnce(now_ms, timeout_ms);
        if (_txStallTimer.timedOut(now_ms)) {
            _txFramer.reset();
            _stream.purgeOutput();
            _txStallTimer.stop();
            _status = Status::StreamTxError;
            return false;
        }
        break;

    case TxFramer::Status::OK:
        _txStallTimer.stop();
        break;
    }

    return true;
}

bool Gateway::writeTransparentRaw(const uint8_t* data, size_t len) noexcept {
    if (data == nullptr) {
        _status = Status::NullPointer;
        return false;
    }
    if (len == 0u) {
        _status = Status::EmptyPayload;
        return false;
    }
    if (!_txFramer.beginRaw(data, len)) {
        _status = Status::TxBusyRaw;
        return false;
    }
    clearError();
    return true;
}

bool Gateway::receive(Message& out) {
    if (_stream.getStatus() == BIF::IByteStream::Status::OverFlowRX) {
        recoverStreamRxOverFlow();
        return false;
    }

    while (_stream.available() > 0u) {
        uint8_t b = 0;
        const size_t n = _stream.read(&b, 1u);
        if (n != 1u) {
            if (_stream.getStatus() != BIF::IByteStream::Status::OK)
                onStreamReadFault(_stream.getStatus());
            break;
        }
        if (_rxFramer.appendByte(b)) {
            clearError();
            TranslateMessage(_rxFramer.frame, out);
            misc::printRxLine(_rxFramer.frame, out);
            return true;
        }
        if (_rxFramer.getOverflowReport())
            _status = Status::RxOverflow;
    }

    if (_stream.getStatus() == BIF::IByteStream::Status::OverFlowRX)
        recoverStreamRxOverFlow();
    return false;
}

} // namespace nex
