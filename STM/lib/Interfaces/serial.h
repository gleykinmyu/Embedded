#pragma once
#include <cstdint>
#include <cstddef>
#include "ringbuffer.h"

// =================================================================
// 1. ПРИКЛАДНОЙ ИНТЕРФЕЙС (IStream)
// Используется в бизнес-логике (парсеры, протоколы)
// =================================================================
class IStream {
public:
    virtual ~IStream() = default;

    /// Записывает до size байт; возвращает фактически записанное число (0 — нет места / ошибка).
    virtual size_t write(const uint8_t* data, size_t size) = 0;
    /// Читает до maxSize байт; возвращает фактически прочитанное число (0 — буфер пуст).
    virtual size_t read(uint8_t* buffer, size_t maxSize) = 0;

    /// Байт в приёмном буфере, доступные для read() без ожидания (аналог Stream::available()).
    virtual size_t available() const = 0;
    /// Свободное место в передающем буфере (сколько байт можно записать минимум одним вызовом write).
    virtual size_t availableWrite() const = 0;

    virtual bool isBusy() const = 0; // Идет ли передача (в буфере или железе)
};

// =================================================================
// 2. СИСТЕМНЫЙ ИНТЕРФЕЙС (ISerial)
// Описывает управление железом и работу прерываний
// =================================================================
class ISerial : public IStream {
public:
    virtual bool init(uint32_t baudrate) = 0;
    virtual bool isInitialized() const = 0;
    virtual void flush() = 0;

    virtual bool hasErrors() const = 0;
    virtual void clearErrors() = 0;

    // Синхронизация (для RAII Guard)
    virtual void lock() = 0;
    virtual void unlock() = 0;

    // Обработчики прерываний (вызываются из ISR)
    virtual void handleTxInterrupt() = 0;
    virtual void handleRxInterrupt() = 0;
};

// =================================================================
// 3. RAII GUARD
// Автоматическое управление критическими секциями
// =================================================================
class SerialGuard {
    ISerial& _s;
public:
    explicit SerialGuard(ISerial& s) : _s(s) { _s.lock(); }
    ~SerialGuard() { _s.unlock(); }
    SerialGuard(const SerialGuard&) = delete;
    SerialGuard& operator=(const SerialGuard&) = delete;
};

// =================================================================
// 4. БАЗОВАЯ РЕАЛИЗАЦИЯ (BufferedSerial)
// Объединяет интерфейсы и буферы, не привязываясь к конкретному МК
// =================================================================
template <size_t TxSize, size_t RxSize>
class BufferedSerial : public ISerial {
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
    size_t availableWrite() const override { return txBuf.space(); }

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
