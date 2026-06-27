#pragma once
#include <stdint.h>
#include <stddef.h>

namespace BIF { // Base Interface

/// Упорядоченный поток байтов (UART/USB CDC, полезная нагрузка RS-485, потоковый SPI и т.п.).
///
/// **Контракт**
/// - `write`/`read` не обязаны быть блокирующими; частичная запись/чтение допустима.
/// - `write()==0` при `isOpen()` — backpressure (нет места в TX-буфере), не `getStatus()`.
/// - `getStatus()` — только ошибки **приёма**; смысл имеет при `isOpen()==true`.
/// - Закрытый порт (`!isOpen()`) — `write`/`read` возвращают 0; lifecycle вне enum Status.
/// - Link-down / HeartBeat — уровень приложения (Nextion NIS не предоставляет ping).

class IByteStream 
{
public:
    virtual ~IByteStream() = default;

    /// Записывает до size байт; возвращает фактически записанное число (0 — нет места / порт закрыт).
    virtual size_t write(const uint8_t* data, size_t size) = 0;

    /// Читает до maxSize байт; возвращает фактически прочитанное число (0 — буфер пуст).
    virtual size_t read(uint8_t* buffer, size_t maxSize) = 0;

    /// Байты в приёмной очереди, доступные для read() без ожидания.
    virtual size_t available() const = 0;

    /// Свободное место под запись (сколько байт можно записать в текущем состоянии).
    virtual size_t availableForWrite() const = 0;

    /// Сбрасывает входной буфер (принятые, ещё не прочитанные байты).
    virtual void purge() = 0;

    /// Сбрасывает выходной буфер (ещё не отправленные байты).
    virtual void purgeOutput() = 0;

    /// Дожидается опустошения выходного буфера (аналог flush на стороне передачи).
    virtual void flush() = 0;

    enum class Status : uint8_t 
    { 
        /// Нормальная работа, ошибок приёма нет.
        OK = 0,
        /// Внутреннее: MCU/буфер не успевает вычитывать RX (кольцо, UART overrun).
        OverFlowRX,
        /// Внешнее: искажения на линии (шум, FE/NE, parity и т.п.).
        DataError,
    };

    /// Порт открыт и готов к обмену (явный open/close драйвера).
    virtual bool isOpen() = 0;

    /// Sticky-ошибки приёма; сброс — `clearErrors()`. При `!isOpen()` — `OK`.
    virtual Status getStatus() = 0;

    /// Сбрасывает флаги ошибок, возвращаемые getStatus().
    virtual void clearErrors() = 0;
};

inline const char* cstr(IByteStream::Status s) noexcept {
    switch (s) {
    case IByteStream::Status::OK: return "OK";
    case IByteStream::Status::OverFlowRX: return "OverFlowRX";
    case IByteStream::Status::DataError: return "DataError";
    default: return "?";
    }
}

} // namespace BIF
