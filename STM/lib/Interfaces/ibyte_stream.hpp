#pragma once
#include <stdint.h>
#include <stddef.h>

namespace BIF { // Base Interface

/// Упорядоченный поток байтов (UART/USB CDC, полезная нагрузка RS-485, потоковый SPI и т.п.).
/// Контракт: write/read не обязаны быть блокирующими; частичная запись/чтение допустима (см. возврат).

class IByteStream 
{
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

    /// Сбрасывает входной буфер (принятые, ещё не прочитанные байты).
    virtual void purge() = 0;

    /// Сбрасывает выходной буфер (ещё не отправленные байты).
    virtual void purgeOutput() = 0;

    /// Дожидается опустошения выходного буфера (аналог flush на стороне передачи).
    virtual void flush() = 0;

    enum class Status : uint8_t 
    { 
        /// Нормальная работа, ошибок нет.
        OK = 0,
        /// Переполнение аппаратного/программного буфера приёма.
        OverFlowRX,
        /// Переполнение аппаратного/программного буфера передачи.
        OverFlowTX,
        /// Ошибка бита при приёме (например, шум, неверный уровень на линии для UART).
        BitError,
        /// Ошибка кадра при приёме: стоп-бит, чётность, break и т.п. (зависит от реализации).
        FrameError,
        /// Линия/канал недоступен: кабель, хост USB, потеря несущей и т.д.
        Disconnected
    };

    /// Поток открыт и готов к обмену (зависит от реализации: порт, сессия и т.д.).
    virtual bool isOpen() = 0;

    /// Текущее состояние линии/драйвера; не сбрасывается до clearErrors().
    virtual Status getStatus() = 0;

    /// Сбрасывает флаги ошибок, возвращаемые getStatus().
    virtual void clearErrors() = 0;
};

} // namespace BIF