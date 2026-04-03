#pragma once
#include <stdint.h>
#include <stddef.h>
#include "byte_stream.hpp"
#include "ringbuffer.hpp"

// =================================================================
// 1. СИСТЕМНЫЙ ИНТЕРФЕЙС (ISerial)
// Описывает управление железом и работу прерываний
// =================================================================
class ISerial : public IByteStream 
{
public:
    virtual bool open(uint32_t baudrate) = 0;

    /// Передача ещё не завершена (TX-буфер и/или железо — по реализации драйвера).
    virtual bool isBusy() const = 0;

    // Синхронизация (для RAII Guard)
    virtual void lock() = 0;
    virtual void unlock() = 0;

    // Обработчики прерываний (вызываются из ISR)
    virtual void handleTxInterrupt() = 0;
    virtual void handleRxInterrupt() = 0;
};

// =================================================================
// 2. RAII GUARD
// Автоматическое управление критическими секциями
// =================================================================
class SerialGuard 
{
    ISerial& _s;

public:
    explicit SerialGuard(ISerial& s) : _s(s) { _s.lock(); }
    ~SerialGuard() { _s.unlock(); }
    SerialGuard(const SerialGuard&) = delete;
    SerialGuard& operator=(const SerialGuard&) = delete;
};

// =================================================================
// 3. БАЗОВАЯ РЕАЛИЗАЦИЯ (BufferedSerial)
// Объединяет интерфейсы и буферы, не привязываясь к конкретному МК
// =================================================================
template <size_t TxSize, size_t RxSize>
class SerialBase : public ISerial 
{
protected:
    RingBuffer<TxSize> txBuf;
    RingBuffer<RxSize> rxBuf;
    volatile bool hwError = false;
    bool initialized = false;

public:
    bool isInitialized() const override { return initialized; }

    size_t write(const uint8_t* data, size_t size) override {
        if (!initialized || !data || size == 0) return 0;

        SerialGuard guard(*this);
        size_t n = 0;
        while (n < size && txBuf.push(data[n])) {
            ++n;
        }
        if (n > 0) {
            startTransmission(); // Железо-зависимый запуск передачи
        }
        return n;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override {
        if (!buffer) return 0;
        SerialGuard guard(*this);
        size_t count = 0;
        while (count < maxSize && rxBuf.pop(buffer[count])) {
            count++;
        }
        return count;
    }

    size_t available() const override { return rxBuf.size(); }
    size_t availableForWrite() const override { return txBuf.space(); }

    /// Базово: данные ещё в программном TX-буфере. Для учёта сдвигового регистра переопределите в драйвере.
    bool isBusy() const override { return txBuf.size() > 0; }

    void flush() override { 
        SerialGuard guard(*this); 
        rxBuf.clear(); 
        txBuf.clear(); 
        stopTransmission(); 
    }

    bool hasErrors() const override { return hwError || rxBuf.overflows() > 0; }
    void clearErrors() override { hwError = false; rxBuf.clearOverflows(); }

    // Логика обработки прерываний
    void handleRxInterrupt() override {
        if (checkHardwareError()) hwError = true;
        uint8_t byte = readHardware();
        rxBuf.push(byte);
    }

    void handleTxInterrupt() override {
        uint8_t byte;
        if (txBuf.pop(byte)) {
            writeHardware(byte);
        } else {
            stopTransmission(); // Буфер пуст, выключаем прерывания TXE/TC
        }
    }

    // Чисто виртуальные методы для реализации в конечном драйвере (STM32/AVR/ESP)
    virtual void startTransmission() = 0;
    virtual void stopTransmission() = 0;
    virtual uint8_t readHardware() = 0;
    virtual void writeHardware(uint8_t data) = 0;
    virtual bool checkHardwareError() = 0;
};
