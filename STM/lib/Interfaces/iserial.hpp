#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ibyte_stream.hpp"
#include "ringbuffer.hpp"

namespace BIF {

// =================================================================
// АППАРАТНЫЙ UART (IHardwareSerial)
// IByteStream — публичный контракт; ниже — то, что реализует драйвер платы.
// =================================================================

class IHardwareSerial : public BIF::IByteStream
{
public:
    //Открытие и закрытие порта.
    virtual bool open(uint32_t baudrate) = 0;
    virtual void close() = 0;

    //Обработчики прерываний для передачи и приема байтов.
    virtual void IRQ_TX_Handler() = 0;
    virtual void IRQ_RX_Handler() = 0;

protected:
    /// Включить прерывание «есть место в TX» / TXE (драйвер); вызывается из write() при появлении данных в кольце.
    virtual void IRQ_TX_Enable() = 0;
    /// Выключить TX IRQ, когда TX-кольцо пусто (обычно из IRQ_TX_Handler).
    virtual void IRQ_TX_Disable() = 0;

    /// true, пока периферия ещё ведёт передачу (сдвиговый регистр / !TC — см. RM для USART).
    virtual bool isHardwareTxBusy() const = 0;

    /// Снять байт с приёмника по правилам RM (часто после чтения статуса в checkErrors).
    virtual uint8_t readHardware() = 0;
    virtual void writeHardware(uint8_t data) = 0;

    /// Прочитать флаги ошибок RX (ISR/SR), выставить у ISerial _isFrameError / _isBitError при необходимости.
    /// Возвращает true, если байт после readHardware() не следует класть в кольцо (кадр/шум и т.п.).
    virtual bool checkErrors() = 0;

    //Захват и освобождение критической секции.
    virtual void lock() = 0;
    virtual void unlock() = 0;

    //RAII guard для синхронизации доступа к аппаратному UART.
    class SGuard
    {
        IHardwareSerial& _s;

    public:
        explicit SGuard(IHardwareSerial& s) : _s(s) { _s.lock(); }
        ~SGuard() { _s.unlock(); }
        SGuard(const SGuard&) = delete;
        SGuard& operator=(const SGuard&) = delete;
    };
};

// =================================================================
// Базовая реализация (ISerial)
// Инвариант TX: при успешной записи в кольцо write() вызывает IRQ_TX_Enable();
// IRQ_TX_Handler выгружает байты в периферию и при пустом кольце вызывает IRQ_TX_Disable().
// flush() ждёт пустого _txBuf (нужен работающий TX IRQ) и затем ждет окончания передачи (isHardwareTxBusy()).
// =================================================================
template <size_t TxSize, size_t RxSize>
class ISerial : public IHardwareSerial
{
private:
    MISC::RingBuffer<TxSize> _txBuf;
    MISC::RingBuffer<RxSize> _rxBuf;

protected:
    volatile bool _isOpen = false;
    volatile bool _isBitError = false;
    volatile bool _isFrameError = false;
    volatile bool _isDisconnected = false;

public:
    bool isOpen() override { return _isOpen; }

    /// Краткий снимок без SGuard (флаги и счётчики переполнения кольца).
    IByteStream::Status getStatus() override
    {
        if (!_isOpen)
            return IByteStream::Status::Disconnected;
        if (_rxBuf.overflows() > 0)
            return IByteStream::Status::OverFlowRX;
        if (_txBuf.overflows() > 0)
            return IByteStream::Status::OverFlowTX;
        if (_isFrameError)
            return IByteStream::Status::FrameError;
        if (_isBitError)
            return IByteStream::Status::BitError;
        if (_isDisconnected)
            return IByteStream::Status::Disconnected;
        return IByteStream::Status::OK;
    }

    /// Кладёт байты в TX-кольцо; при n > 0 включает TX IRQ — драйвер должен опустошать кольцо в IRQ_TX_Handler.
    size_t write(const uint8_t* data, size_t size) override
    {
        if (!_isOpen || !data || size == 0)
            return 0;

        SGuard guard(*this);
        size_t n = 0;
        while (n < size && _txBuf.push(data[n])) {
            ++n;
        }
        if (n > 0)
            IRQ_TX_Enable();
        return n;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override {
        if (!buffer) return 0;
        SGuard guard(*this);
        size_t count = 0;
        while (count < maxSize && _rxBuf.pop(buffer[count])) {
            count++;
        }
        return count;
    }

    size_t available() const override { return _rxBuf.size(); }
    size_t availableForWrite() const override { return _txBuf.space(); }

    /// Сбрасывает приёмный буфер; счётчик переполнений rx не трогаем (см. getStatus / clearErrors).
    void purge() override {
        SGuard guard(*this);
        _rxBuf.clearData();
    }

    void purgeOutput() override {
        SGuard guard(*this);
        _txBuf.clearData();
        IRQ_TX_Disable();
    }

    /// Ожидание пустого _txBuf (требуется, чтобы TX IRQ реально обрабатывался) и завершения передачи (isHardwareTxBusy).
    /// При !_isOpen — немедленный выход (кольцо при отключённом IRQ могло бы не опустошиться).
    void flush() override
    {
        if (!_isOpen)
            return;
        while (_txBuf.size() > 0) {
        }
        while (isHardwareTxBusy()) {
        }
    }

    void clearErrors() override
    {
        _isBitError = false;
        _isFrameError = false;
        _isDisconnected = false;
        _rxBuf.clearOverflows();
        _txBuf.clearOverflows();
    }

    void IRQ_RX_Handler() override
    {
        const bool discardByte = checkErrors();
        const uint8_t byte = readHardware();
        if (!discardByte)
            _rxBuf.push(byte);
    }

    /// Выгрузка TX-кольца в периферию; при пустом кольце отключает TX IRQ.
    void IRQ_TX_Handler() override
    {
        uint8_t byte;
        if (_txBuf.pop(byte))
            writeHardware(byte);
        else
            IRQ_TX_Disable();
    }
};

} // namespace BIF
