#include "../nexGateway.hpp"

namespace nex
{
//===============================================
// RxFramer
//===============================================

void RxFramer::reset() {
    _state = State::WaitHeader;
    _terms = 0;
    frame.length = 0;
}

bool RxFramer::appendByte(uint8_t byte) {
    switch (_state)
    {
    // NIS §18 — ожидание заголовка кадра.
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
        } else if (frame.length < RxFrame::MAX_PAYLOAD)
            frame.payload[frame.length++] = byte;
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
            for (uint8_t i = 0; i < _terms; i++) frame.payload[frame.length++] = Physical::TERM_BYTE;
            frame.payload[frame.length++] = byte;
            _terms = 0;
            _state = State::Collect;
        }
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
        out = msg::SystemEvent{msg::SystemEvent::Code::StartupPreamble};
        return;
    }

    const uint8_t h = f.header;

    // NIS §8 — статус ответа (успешно, нет такого компонента, нет такой страницы, нет такого файла и т.п.).
    if (h <= static_cast<uint8_t>(msg::StatusResponse::Code::VariableNameTooLong)) {
        out = msg::StatusResponse{static_cast<msg::StatusResponse::Code>(h)};
        return;
    }

    // NIS §9 — строковый ответ.
    if (h == msg::StringResponse::Header) {
        msg::StringResponse s{};
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
    if (h == msg::NumericResponse::Header) {
        msg::NumericResponse n{};
        n.value = 0;
        if (f.length >= 4u)
            std::memcpy(&n.value, f.payload, 4u);
        out = n;
        return;
    }

    // NIS §11 — событие нажатия на компонент.
    if (h == msg::TouchCompEvent::Header) {
        msg::TouchCompEvent t{};
        t.page_id = f.length > 0u ? f.payload[0] : 0u;
        t.component_id = f.length > 1u ? f.payload[1] : 0u;
        t.state = f.length > 2u ? static_cast<msg::TouchState>(f.payload[2]) : msg::TouchState::Release;
        out = t;
        return;
    }

    // NIS §12 — событие нажатия на экран.
    if (h == static_cast<uint8_t>(msg::TouchPlane::Awake) || h == static_cast<uint8_t>(msg::TouchPlane::Sleep)) {
        msg::TouchXYEvent e{};
        e.plane = (h == static_cast<uint8_t>(msg::TouchPlane::Awake)) ? msg::TouchPlane::Awake : msg::TouchPlane::Sleep;
        if (f.length >= 5u) {
            const uint16_t rx = static_cast<uint16_t>((uint16_t(f.payload[0]) << 8) | f.payload[1]);
            const uint16_t ry = static_cast<uint16_t>((uint16_t(f.payload[2]) << 8) | f.payload[3]);
            e.pos.x = static_cast<Coord>(rx);
            e.pos.y = static_cast<Coord>(ry);
            e.state = static_cast<msg::TouchState>(f.payload[4]);
        } else {
            e.pos = {};
            e.state = msg::TouchState::Release;
        }
        out = e;
        return;
    }

    // NIS §13 — событие смены страницы.
    if (h == msg::PageEvent::Header) {
        out = msg::PageEvent{static_cast<uint8_t>(f.length > 0u ? f.payload[0] : 0u)};
        return;
    }

    // NIS §14 — системное событие.
    if (h == static_cast<uint8_t>(msg::SystemEvent::Code::SerialBufferOverflow)
        || (h >= static_cast<uint8_t>(msg::SystemEvent::Code::AutoEnteredSleepMode)
            && h <= static_cast<uint8_t>(msg::SystemEvent::Code::StartMicroSdUpgrade))
        || h == static_cast<uint8_t>(msg::SystemEvent::Code::TransparentBlockComplete)
        || h == static_cast<uint8_t>(msg::SystemEvent::Code::TransparentReadyToReceive)) {
        out = msg::SystemEvent{static_cast<msg::SystemEvent::Code>(h)};
        return;
    }

    // NIS §15 — неизвестное сообщение.
    out = msg::Unknown{msg::Unknown::Reason::UnrecognizedHeader, h};
}

//===============================================
// TxFramer
//===============================================

void TxFramer::reset() noexcept {
    _state = State::Idle;
    frame.length = 0;
    _pos = 0;
}

bool TxFramer::isIdle() const noexcept {
    return _state == State::Idle && frame.length == 0u;
}

bool TxFramer::tick(BIF::IByteStream& stream) noexcept {
    if (_state == State::Idle && frame.length > 0u)
    {
        std::memcpy(frame.payload + frame.length, Physical::FRAME_TERMINATORS, Physical::TERM_COUNT);
        frame.length += Physical::TERM_COUNT;
        _state = State::Payload;
    }

    while (_state == State::Payload) {
        const size_t w = stream.write(frame.payload + _pos, static_cast<size_t>(frame.length - _pos));
        if (w == 0u) {
            if (stream.getStatus() != BIF::IByteStream::Status::OK) {
                reset();
                stream.purgeOutput();
                return false;
            }
            return true;
        }
        _pos = static_cast<uint16_t>(_pos + static_cast<uint16_t>(w));
        if (_pos >= frame.length) {
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

bool Gateway::pushCommand(const Command& cmd) {
    if (!_txFramer.isIdle())
        return false;

    if (!cmd.serialize(_txFramer.frame)) {
        _txFramer.reset();
        return false;
    }

    if (_txFramer.frame.length == 0u)
        return false;

    return transmit();
}

bool Gateway::pushTransparentPreamble(const TransparentCommand& cmd) {
    return pushCommand(cmd);
}

bool Gateway::transmit() noexcept {
    return _txFramer.tick(_stream);
}

size_t Gateway::writeTransparentRaw(const uint8_t* data, size_t len) noexcept {
    if (data == nullptr || len == 0u)
        return 0u;
    return _stream.write(data, len);
}

bool Gateway::receive(Message& out) {
    while (_stream.available() > 0u) {
        uint8_t b = 0;
        const size_t n = _stream.read(&b, 1u);
        if (n != 1u) {
            // Не удалось прочитать байт: при ошибке канала сбрасываем незавершённый кадр и очищаем вход,
            // иначе оставшийся мусор исказит следующий ответ.
            if (_stream.getStatus() != BIF::IByteStream::Status::OK) {
                _rxFramer.reset();
                _stream.purge();
            }
            break;
        }
        if (_rxFramer.appendByte(b)) {
            // Собран полный кадр — данные в `_rxFramer.frame` до следующего `appendByte`.
            TranslateMessage(_rxFramer.frame, out);
            return true;
        }
    }
    return false;
}

} // namespace nex
