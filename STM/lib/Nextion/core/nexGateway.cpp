#include "nexGateway.hpp"

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

bool RxFramer::appendByte(uint8_t byte) {
    switch (_state)
    {
    // NIS §18 — ожидание заголовка кадра (первый байт после `0xFF×3`, не «скользящее» окно).
    case State::WaitHeader:
        frame.header = byte;
        frame.length = 0;
        _terms = 0;
        _state = State::Collect;
        break;

    // NIS §17 — сборка полезной нагрузки кадра.
    case State::Collect:
        if (byte == Physical::TERM_BYTE) {
            _terms = 1;
            _state = State::WaitTerm;
        } else if (frame.length < RxFrame::MAX_PAYLOAD) {
            frame.payload[frame.length++] = byte;
        } else {
            enterResync(byte);
        }
        break;

    // NIS §16 — ожидание терминальных байт (обычно 0xFF×3). §1.16 transparent. См. Instruction Set.
    case State::WaitTerm:
        if (byte == Physical::TERM_BYTE) {
            if (++_terms >= Physical::TERM_COUNT) {
                _state = State::WaitHeader;
                _terms = 0;
                return true;
            }
        } else {
            for (uint8_t i = 0; i < _terms; i++) {
                if (frame.length < RxFrame::MAX_PAYLOAD)
                    frame.payload[frame.length++] = Physical::TERM_BYTE;
                else {
                    enterResync(byte);
                    return false;
                }
            }
            if (frame.length < RxFrame::MAX_PAYLOAD)
                frame.payload[frame.length++] = byte;
            else
                enterResync(byte);
            if (_state == State::Resync)
                return false;
            _terms = 0;
            _state = State::Collect;
        }
        break;

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
    // NIS §7 — первый байт (обычно 0xFF×3). §1.16 transparent. См. Instruction Set.
    if (f.header == 0u && f.length >= 2u) {
        out = msg::evSystem{msg::evSystem::Code::StartupPreamble};
        return;
    }

    const uint8_t h = f.header;

    // NIS §8 — статус ответа (успешно, нет такого компонента, переполнение буфера и т.п.).
    if (h <= static_cast<uint8_t>(msg::Status::Code::Serial_Overflow)) {
        out = msg::Status{static_cast<msg::Status::Code>(h)};
        return;
    }

    // NIS §9 — строковый ответ.
    if (h == msg::getString::Header) {
        msg::getString s{};
        const uint16_t len = f.length;
        const uint16_t n = len < RxFrame::MAX_PAYLOAD ? len : RxFrame::MAX_PAYLOAD;
        s.length = len;
        if (n > 0u)
            std::memcpy(s.chars, f.payload, n);
        if (n < RxFrame::MAX_PAYLOAD)
            s.chars[n] = '\0';
        out = s;
        return;
    }

    // NIS §10 — числовой ответ.
    if (h == msg::getNumeric::Header) {
        msg::getNumeric n{};
        n.value = 0;
        if (f.length >= 4u)
            std::memcpy(&n.value, f.payload, 4u);
        out = n;
        return;
    }

    // NIS §11 — событие нажатия на компонент.
    if (h == msg::evTouch::Header) {
        msg::evTouch t{};
        t.page_id = f.length > 0u ? f.payload[0] : 0u;
        t.comp_id = f.length > 1u ? f.payload[1] : 0u;
        t.state = f.length > 2u ? static_cast<TouchState>(f.payload[2]) : TouchState::Release;
        out = t;
        return;
    }

    // NIS §12 — событие нажатия на экран.
    if (h == static_cast<uint8_t>(msg::evTouchXY::Mode::Awake) || h == static_cast<uint8_t>(msg::evTouchXY::Mode::Sleep)) {
        msg::evTouchXY e{};
        e.mode = (h == static_cast<uint8_t>(msg::evTouchXY::Mode::Awake)) ? msg::evTouchXY::Mode::Awake : msg::evTouchXY::Mode::Sleep;
        if (f.length >= 5u) {
            const Coord rx = static_cast<Coord>((uint16_t(f.payload[0]) << 8) | f.payload[1]);
            const Coord ry = static_cast<Coord>((uint16_t(f.payload[2]) << 8) | f.payload[3]);
            e.pos.x = rx;
            e.pos.y = ry;
            e.state = static_cast<TouchState>(f.payload[4]);
        } else {
            e.pos = {};
            e.state = TouchState::Release;
        }
        out = e;
        return;
    }

    // NIS §13 — событие смены страницы.
    if (h == msg::evPage::Header) {
        out = msg::evPage{static_cast<uint8_t>(f.length > 0u ? f.payload[0] : 0u)};
        return;
    }

    // NIS §1.16 — Transparent Data Mode.
    if (h == static_cast<uint8_t>(msg::evTransparent::Code::BlockComplete)
        || h == static_cast<uint8_t>(msg::evTransparent::Code::ReadyToReceive)) {
        out = msg::evTransparent{static_cast<msg::evTransparent::Code>(h)};
        return;
    }

    // NIS §14 — системное событие.
    if (h >= static_cast<uint8_t>(msg::evSystem::Code::AutoEnteredSleepMode)
        && h <= static_cast<uint8_t>(msg::evSystem::Code::StartMicroSdUpgrade)) {
        out = msg::evSystem{static_cast<msg::evSystem::Code>(h)};
        return;
    }

    // NIS §15 — неизвестный заголовок кадра (внутренний код `Status`, не с шины).
    (void)h;
    out = msg::Status{msg::Status::Code::Unrecognized_Header};
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

bool TxFramer::tick(BIF::IByteStream& stream) noexcept {
    if (_state == State::Idle && frame.length > 0u)
    {
        std::memcpy(frame.payload + frame.length, Physical::FRAME_TERMINATORS, Physical::TERM_COUNT);
        frame.length += Physical::TERM_COUNT;
        _state = State::FramePayload;
    }

    while (_state == State::FramePayload) {
        const size_t w = stream.write(frame.payload + _pos, static_cast<size_t>(frame.length - _pos));
        if (w == 0u) {
            if (stream.getStatus() != BIF::IByteStream::Status::OK) {
                reset();
                stream.purgeOutput();
                return false;
            }
            return true;
        }
        _pos += w;
        if (_pos >= frame.length) {
            reset();
            return true;
        }
    }

    while (_state == State::RawPayload) {
        const size_t w = stream.write(_rawData + _pos, _rawLen - _pos);
        if (w == 0u) {
            if (stream.getStatus() != BIF::IByteStream::Status::OK) {
                reset();
                stream.purgeOutput();
                return false;
            }
            return true;
        }
        _pos += w;
        if (_pos >= _rawLen) {
            reset();
            return true;
        }
    }
    return true;
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
    _rxFramer.reset();
    _stream.purge();
    _status = Status::StreamRxError;
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

    clearError();
    return true;
}

bool Gateway::transmit() noexcept {
    if (_txFramer.tick(_stream))
        return true;
    _status = Status::StreamTxError;
    return false;
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
