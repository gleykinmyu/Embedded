#pragma once
#include <stdint.h>
#include <stddef.h>

#include "ilockable.hpp"

namespace BIF { // Base Interface

/// Упорядоченный поток байтов (UART/USB CDC, полезная нагрузка RS-485, потоковый SPI и т.п.).
///
/// **Контракт**
/// - `write`/`read` не обязаны быть блокирующими; частичная запись/чтение допустима.
/// - `write()==0` при `isOpen()` — backpressure (нет места в TX-буфере), не `getStatus()`.
/// - `getStatus()` — только ошибки **приёма**; смысл имеет при `isOpen()==true`.
/// - `open` / `close` — жизненный цикл порта. Закрытый порт (`!isOpen()`) — `write`/`read` возвращают 0.
/// - Ошибки приёма живут в `Status`, не в `open`/`close`.
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

    /// Включить поток. `baud == 0` — false. Для UART это битрейт, для SPI — тактовая.
    virtual bool open(uint32_t baud) = 0;

    /// Выключить поток. Повторный вызов допустим.
    virtual void close() = 0;

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

using IrqCallback = void (*)(void* ctx) noexcept;

/// Аппаратный UART. Колец нет: байт и getStatus() — текущее rx-событие.
/// Колбэки живут здесь; наследник в прерывании вызывает invoke*.
class IHWByteStream : public ILockable
{
public:
    virtual ~IHWByteStream() = default;

    /// USART и вектор. baud == 0 — false. Биты RX/TX прерываний не включает.
    virtual bool open(uint32_t baud) = 0;
    /// Снять оба бита прерываний и выключить USART. Колбэки сохраняются.
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /// ctx — this клиента. nullptr снимает колбэк.
    void setRxCallback(IrqCallback fn, void* ctx) noexcept { _rx.bind(fn, ctx); }
    void setTxCallback(IrqCallback fn, void* ctx) noexcept { _tx.bind(fn, ctx); }

    /// Читает DR текущего rx-события (снимает RXNE / FE). Вызывать из rx-колбэка.
    virtual uint8_t readByte() const = 0;

    /// Статус этого байта, не липкий.
    /// OverFlowRX — overrun; иначе DataError — FE, шум или parity; иначе OK.
    /// Оба сразу — OverFlowRX.
    virtual IByteStream::Status getStatus() const = 0;

    /// Один байт в DR. false — регистр ещё занят. TX-прерывание само не включает.
    virtual bool writeByte(uint8_t data) = 0;

    virtual void irqRxEnable() = 0;  ///< RXNE / RXC
    virtual void irqRxDisable() = 0;
    virtual void irqTxEnable() = 0;  ///< TXE / UDRE
    virtual void irqTxDisable() = 0;

    /// true, пока сдвиговый регистр ещё передаёт.
    virtual bool isTxBusy() const = 0;

protected:
    struct Callback {
        IrqCallback volatile fn = nullptr;
        void* volatile ctx = nullptr;

        void bind(IrqCallback f, void* c) noexcept
        {
            if (f == nullptr) {
                fn = nullptr;
                ctx = nullptr;
                return;
            }
            ctx = c;
            fn = f;
        }

        /// Драйвер вызывает после чтения SR, затем DR (_rx) или когда DR передачи пуст (_tx).
        void invoke() const noexcept
        {
            const IrqCallback f = fn;
            void* const c = ctx;
            if (f != nullptr)
                f(c);
        }
    };

    Callback _rx{};
    Callback _tx{};
};

/// UART, который сам держит Break: TX в 0 на breakUs, затем снова периферия
/// и пауза mabUs при idle. Наследник знает свою TX-ногу.
/// Вызов из потока, не из прерывания этого UART.
class IBreakByteStream : public IHWByteStream
{
public:
    virtual void sendBreak(uint32_t breakUs, uint32_t mabUs = 0) = 0;
};

} // namespace BIF
