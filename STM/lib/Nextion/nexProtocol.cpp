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

bool FrameParser::feed(uint8_t inputByte, Frame& outFrame) {
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
        } else if (_current.length < Frame::MAX_PAYLOAD)
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

void TranslateMessage(const Frame& f, Message& out) 
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
        const uint16_t n = len < Frame::MAX_PAYLOAD ? len : Frame::MAX_PAYLOAD;
        s.length = len;
        if (n > 0u)
            std::memcpy(s.chars, f.payload, n);
        if (n < Frame::MAX_PAYLOAD)
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

bool NexGate::write_all(BIF::IByteStream& s, const uint8_t* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        const size_t w = s.write(data + off, len - off);
        // 0 байт: ошибка канала (см. getStatus) или TX переполнен в этом неблокирующем вызове.
        if (w == 0u)
            return false;
        off += w;
    }
    return true;
}

bool NexGate::transmit(const char* c, uint32_t nowMs) {
    // Сбой write_all: в SW TX уже лежит начало текущей команды без полного кадра (нет 0xFF×3).
    // Без purgeOutput() следующий вызов допишет новую команду к этому хвосту — на Nextion уйдёт мусор.
    // «Остаток старой» команды в нормальном цикле здесь не ожидается: после успешного transmit()
    // flush() дожидается опустошения TX; иначе это уже аварийное состояние канала, и сброс очереди
    // передачи — осознанное восстановление, а не потеря заведомо целого кадра.
    const uint8_t* p = (uint8_t*)(c);
    const size_t len = std::strlen(c);
    if (!write_all(_stream, p, len)) {
        _isWaiting = false;
        _txAttempt = 0;
        _stream.purgeOutput();
        return false;
    }
    if (!write_all(_stream, Physical::FRAME_TERMINATORS, Physical::TERM_COUNT)) {
        _isWaiting = false;
        _txAttempt = 0;
        _stream.purgeOutput();
        return false;
    }
    _stream.flush();
    _lastTxMs = nowMs;
    _isWaiting = true;
    return true;
}

bool NexGate::send(const char* cmd, uint32_t nowMs) {
    if (Config::RETRY_BUF > 0u) {
        _retryBuf[0] = '\0';
        std::strncpy(_retryBuf, cmd, Config::RETRY_BUF - 1u);
        _retryBuf[Config::RETRY_BUF - 1u] = '\0';
    }
    _txAttempt = 1;
    return transmit(_retryBuf, nowMs);
}

bool NexGate::update(Message& out, uint32_t nowMs, bool* send_aborted) {
    // Сбрасываем флаг «передача сорвана»; вызывающий код узнает об отмене только при исчерпании повторов.
    if (send_aborted)
        *send_aborted = false;

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
        Frame f = {};
        if (_framer.feed(b, f)) {
            // Собран полный кадр (заголовок + полезная нагрузка + терминатор 0xFF×3) — преобразуем в Message.
            TranslateMessage(f, out);
            _isWaiting = false;
            _txAttempt = 0;
            return true;
        }
    }

    // Таймаут ответа после send(): если панель молчит дольше TIMEOUT_MS, повторяем команду или сдаёмся.
    if (_isWaiting && Config::TIMEOUT_MS > 0u) {
        const uint32_t elapsed = nowMs - _lastTxMs;
        if (elapsed >= Config::TIMEOUT_MS) {
            const uint8_t maxAttempts = (Config::SEND_RETRY_MAX == 0u) ? 1u : Config::SEND_RETRY_MAX;
            if (_txAttempt < maxAttempts) {
                ++_txAttempt;
                _framer.reset();
                _stream.purge();
                if (!transmit(_retryBuf, nowMs)) {
                    // transmit() уже сбросил ожидание и TX; чистим приём и сигнализируем как при исчерпании повторов.
                    _framer.reset();
                    _stream.purge();
                    if (send_aborted)
                        *send_aborted = true;
                }
            } else {
                // Лимит попыток исчерпан: сбрасываем состояние ожидания; при необходимости сигнализируем вызывающему коду.
                _isWaiting = false;
                _txAttempt = 0;
                _framer.reset();
                _stream.purge();
                if (send_aborted)
                    *send_aborted = true;
            }
        }
    }
    // Ни одного завершённого кадра в этом тике; out не трогаем (кроме случая return true выше).
    return false;
}

} // namespace Nextion
