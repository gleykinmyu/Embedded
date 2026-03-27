#pragma once
#include <cstdint>
#include <cstddef>

// =================================================================
// 1. ПРИКЛАДНОЙ ИНТЕРФЕЙС (IStream)
// Используется в бизнес-логике (парсеры, протоколы)
// =================================================================
class IStream {
public:
    virtual ~IStream() = default;

    virtual bool send(const uint8_t* data, size_t size) = 0;
    virtual size_t receive(uint8_t* buffer, size_t maxSize) = 0;

    virtual size_t availableForRead() const = 0;
    virtual size_t availableForWrite() const = 0;
    
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
// 4. КОЛЬЦЕВОЙ БУФЕР
// Легковесная структура данных для прерываний
// =================================================================
template <size_t Size>
class RingBuffer {
    uint8_t buffer[Size];
    volatile size_t head = 0;
    volatile size_t tail = 0;
    volatile size_t overflowCount = 0;

public:
    bool put(uint8_t data) {
        size_t next = (head + 1) % Size;
        if (next == tail) {
            overflowCount++;
            return false;
        }
        buffer[head] = data;
        head = next;
        return true;
    }

    bool get(uint8_t& data) {
        if (head == tail) return false;
        data = buffer[tail];
        tail = (tail + 1) % Size;
        return true;
    }

    size_t available() const { return (head >= tail) ? (head - tail) : (Size - tail + head); }
    size_t freeSpace() const { return (Size - 1) - available(); }
    void clear() { head = tail = 0; overflowCount = 0; }
    size_t getOverflows() const { return overflowCount; }
    void resetOverflows() { overflowCount = 0; }
};

// =================================================================
// 5. БАЗОВАЯ РЕАЛИЗАЦИЯ (BufferedSerial)
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

    bool send(const uint8_t* data, size_t size) override {
        if (!initialized || !data || size == 0) return false;
        
        SerialGuard guard(*this);
        if (size > txBuf.freeSpace()) return false;

        for (size_t i = 0; i < size; ++i) {
            txBuf.put(data[i]);
        }
        
        startTransmission(); // Железо-зависимый запуск передачи
        return true;
    }

    size_t receive(uint8_t* buffer, size_t maxSize) override {
        if (!buffer) return 0;
        SerialGuard guard(*this);
        size_t count = 0;
        while (count < maxSize && rxBuf.get(buffer[count])) {
            count++;
        }
        return count;
    }

    size_t availableForRead() const override { return rxBuf.available(); }
    size_t availableForWrite() const override { return txBuf.freeSpace(); }
    
    void flush() override { 
        SerialGuard guard(*this); 
        rxBuf.clear(); 
        txBuf.clear(); 
        stopTransmission(); 
    }

    bool hasErrors() const override { return hwError || rxBuf.getOverflows() > 0; }
    void clearErrors() override { hwError = false; rxBuf.resetOverflows(); }

    // Логика обработки прерываний
    void handleRxInterrupt() override {
        if (checkHardwareError()) hwError = true;
        uint8_t byte = readHardware();
        rxBuf.put(byte);
    }

    void handleTxInterrupt() override {
        uint8_t byte;
        if (txBuf.get(byte)) {
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
