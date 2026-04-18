#include "nexProtocol.hpp"
#include "../Interfaces/ibyte_stream.hpp"
#include <cstring>

namespace Nextion 
{
//===============================================
// FrameParser
//===============================================

void FrameParser::reset() {
    _state = State::WaitHeader;
    _terms = 0;
    _current.length = 0;
}

bool FrameParser::feed(uint8_t inputByte, InputFrame& outFrame) {
    switch (_state) 
    {
    // NIS §18 — ожидание заголовка кадра.
    case State::WaitHeader:
        _current.header = inputByte;
        _current.length = 0;
        _terms = 0;
        _state = State::Collect;
        break;

    // NIS §17 — сборка полезной нагрузки кадра.
    case State::Collect:
        if (inputByte == Physical::TERM_BYTE) {
            _terms = 1;
            _state = State::WaitTerm;
        } else if (_current.length < InputFrame::MAX_PAYLOAD)
            _current.payload[_current.length++] = inputByte;
        break;

    // NIS §16 — ожидание терминальных байт (обычно 0xFF×3). §1.16 transparent. См. Instruction Set.
    case State::WaitTerm:
        if (inputByte == Physical::TERM_BYTE) {
            if (++_terms >= Physical::TERM_COUNT) {
                outFrame = _current;
                reset();
                return true;
            }
        } else {
            for (uint8_t i = 0; i < _terms; i++) _current.payload[_current.length++] = Physical::TERM_BYTE;
            _current.payload[_current.length++] = inputByte;
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

void TranslateMessage(const InputFrame& f, Message& out) 
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
        const uint16_t n = len < InputFrame::MAX_PAYLOAD ? len : InputFrame::MAX_PAYLOAD;
        s.length = len;
        if (n > 0u)
            std::memcpy(s.chars, f.payload, n);
        if (n < InputFrame::MAX_PAYLOAD)
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
            e.x = static_cast<uint16_t>((uint16_t(f.payload[0]) << 8) | f.payload[1]);
            e.y = static_cast<uint16_t>((uint16_t(f.payload[2]) << 8) | f.payload[3]);
            e.state = static_cast<msg::TouchState>(f.payload[4]);
        } else {
            e.x = 0u;
            e.y = 0u;
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
// NexGate
//===============================================

NexGate::NexGate(BIF::IByteStream& s) : _stream(s) {}

void NexGate::pumpTx() {
    while (_txPos < _txTotal) {
        if (_stream.getStatus() != BIF::IByteStream::Status::OK) {
            _txPos = 0;
            _txTotal = 0;
            _stream.purgeOutput();
            return;
        }
        const size_t chunk = static_cast<size_t>(_txTotal - _txPos);
        const size_t w = _stream.write(_txPending + _txPos, chunk);
        if (w == 0u)
            return;
        _txPos = static_cast<uint16_t>(_txPos + static_cast<uint16_t>(w));
    }
}

bool NexGate::requestCommand(const char* cmd, size_t len) {
    if (_txPos < _txTotal)
        return false;
    if (len > 0u && cmd == nullptr)
        return false;
    if (len + Physical::TERM_COUNT > static_cast<size_t>(TX_CMD_CAP))
        return false;
    if (len > 0u)
        std::memcpy(_txPending, cmd, len);
    std::memcpy(_txPending + len, Physical::FRAME_TERMINATORS, Physical::TERM_COUNT);
    _txTotal = static_cast<uint16_t>(len + Physical::TERM_COUNT);
    _txPos = 0;
    pumpTx();
    return true;
}

bool NexGate::requestCommand(const char* cmd) {
    if (cmd == nullptr)
        return false;
    return requestCommand(cmd, std::strlen(cmd));
}

bool NexGate::update(Message& out) {
    pumpTx();
    
    // Приём: за один вызов забираем всё, что уже лежит в буфере потока, по байту в парсер кадра Nextion.
    while (_stream.available() > 0u) {
        uint8_t b = 0;
        const size_t n = _stream.read(&b, 1u);
        if (n != 1u) {
            // Не удалось прочитать байт: при ошибке канала сбрасываем незавершённый кадр и очищаем вход,
            // иначе оставшийся мусор исказит следующий ответ.
            if (_stream.getStatus() != BIF::IByteStream::Status::OK) {
                _framer.reset();
                _stream.purge();
            }
            break;
        }
        InputFrame f = {};
        if (_framer.feed(b, f)) {
            // Собран полный кадр (заголовок + полезная нагрузка + терминатор 0xFF×3) — преобразуем в Message.
            TranslateMessage(f, out);
            return true;
        }
    }
    return false;
}

} // namespace Nextion
