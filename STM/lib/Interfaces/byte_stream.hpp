#pragma once
#include <stdint.h>
#include <stddef.h>

/// Упорядоченный поток байтов (UART/USB CDC, полезная нагрузка RS-485, потоковый SPI и т.п.).
/// Контракт: write/read не обязаны быть блокирующими; частичная запись/чтение допустима (см. возврат).

class IByteStream {
public:
    virtual ~IByteStream() = default;

    /// Записывает до size байт; возвращает фактически записанное число (0 — нет места / ошибка).
    virtual size_t write(const uint8_t* data, size_t size) = 0;

    /// Читает до maxSize байт; возвращает фактически прочитанное число (0 — буфер пуст).
    virtual size_t read(uint8_t* buffer, size_t maxSize) = 0;

    /// Байты в приёмной очереди, доступные для read() без ожидания.
    virtual size_t available() const = 0;

    /// Свободное место под запись (сколько байт можно записать в текущем состоянии).
    virtual size_t availableForWrite() const = 0;

    virtual void purge() = 0;

    virtual void purgeOutput() = 0;

    virtual void flush() = 0; //flushOutput

    enum class Status : uint8_t 
    { 
        OK = 0,
        OverFlow, 
        BitError, 
        FrameError,
        Disconnected
    };

    virtual bool isOpen() = 0;

    virtual Status getStatus() = 0;

    virtual void clearErrors() = 0;
};