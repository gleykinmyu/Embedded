#pragma once

/**
 * BIF::IByteStream для UART0..UART2 через драйвер ESP-IDF.
 * uart_driver_install держит ISR и оба кольца. Порог RX FIFO — 120, пачка за прерывание.
 * write() возвращается, когда байты лежат в TX-кольце; в FIFO их перекладывает ISR.
 * Если кольцо полно, write() ждёт, пока ISR освободит место.
 * Пины: InitPins(tx, rx) до open(). Кадр: setFrameFormat при закрытом порте.
 * UART0 обычно занят консолью лога; uart_driver_install тогда не встаёт и open() — false.
 */
#include "ibyte_stream.hpp"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stddef.h>
#include <stdint.h>

namespace Usart {

enum class DataBits : uint8_t { Bits8 = 8 };
enum class Parity : uint8_t { None, Even, Odd };
enum class StopBits : uint8_t { One = 1, Two = 2 };

struct Frame {
    DataBits data_bits;
    Parity parity;
    StopBits stop_bits;

    static constexpr Frame make(DataBits d, Parity p, StopBits s) noexcept
    {
        return {d, p, s};
    }

    static constexpr Frame _8N1() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::One);
    }

    static constexpr Frame _8N2() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::Two);
    }
};

template <uart_port_t N, size_t TxSize = 256, size_t RxSize = 256>
class Serial : public BIF::IByteStream {
    static_assert(N >= UART_NUM_0 && N < UART_NUM_MAX, "UART index");
    static_assert(TxSize > SOC_UART_FIFO_LEN, "TX-кольцо драйвера больше аппаратного FIFO");
    static_assert(RxSize > SOC_UART_FIFO_LEN, "RX-кольцо драйвера больше аппаратного FIFO");

    static constexpr int kEventQueueLen = 32;

    Frame _frame = Frame::_8N1();
    QueueHandle_t _events = nullptr;
    int _txPin = UART_PIN_NO_CHANGE;
    int _rxPin = UART_PIN_NO_CHANGE;
    bool _havePins = false;
    bool _isOpen = false;
    mutable bool _overflow = false;
    mutable bool _dataError = false;

public:
    void InitPins(gpio_num_t tx, gpio_num_t rx)
    {
        _txPin = static_cast<int>(tx);
        _rxPin = static_cast<int>(rx);
        _havePins = true;
        if (_isOpen)
            uart_set_pin(N, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }

    ~Serial() override
    {
        if (_isOpen)
            close();
    }

    bool open(uint32_t baudrate)
    {
        if (_isOpen || baudrate == 0u || !configure(baudrate, _frame))
            return false;

        QueueHandle_t queue = nullptr;
        if (uart_driver_install(N, static_cast<int>(RxSize), static_cast<int>(TxSize), kEventQueueLen, &queue, 0) != ESP_OK)
            return false;

        _events = queue;
        clearErrors();
        _isOpen = true;
        return true;
    }

    bool setFrameFormat(const Frame& fmt) noexcept
    {
        if (_isOpen)
            return false;
        _frame = fmt;
        return true;
    }

    void close()
    {
        if (_isOpen)
            uart_driver_delete(N);
        _events = nullptr;
        _isOpen = false;
    }

    size_t write(const uint8_t* data, size_t size) override
    {
        if (!_isOpen || data == nullptr || size == 0u)
            return 0;
        noteEvents();
        const int n = uart_write_bytes(N, data, size);
        return n > 0 ? static_cast<size_t>(n) : 0u;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override
    {
        if (!_isOpen || buffer == nullptr || maxSize == 0u)
            return 0;
        noteEvents();
        const int n = uart_read_bytes(N, buffer, static_cast<uint32_t>(maxSize), 0);
        return n > 0 ? static_cast<size_t>(n) : 0u;
    }

    size_t available() const override
    {
        if (!_isOpen)
            return 0;
        noteEvents();
        size_t n = 0;
        if (uart_get_buffered_data_len(N, &n) != ESP_OK)
            return 0;
        return n;
    }

    size_t availableForWrite() const override
    {
        if (!_isOpen)
            return 0;
        size_t n = 0;
        if (uart_get_tx_buffer_free_size(N, &n) != ESP_OK)
            return 0;
        return n;
    }

    void purge() override
    {
        if (!_isOpen)
            return;
        uart_flush_input(N);
    }

    void purgeOutput() override
    {
        // Драйвер IDF не отдаёт сброс TX-кольца: уже принятые write() байты уйдут в линию.
    }

    void flush() override
    {
        if (!_isOpen)
            return;
        noteEvents();
        uart_wait_tx_done(N, portMAX_DELAY);
    }

    bool isOpen() override { return _isOpen; }

    Status getStatus() override
    {
        if (!_isOpen)
            return Status::OK;
        noteEvents();
        if (_overflow)
            return Status::OverFlowRX;
        if (_dataError)
            return Status::DataError;
        return Status::OK;
    }

    void clearErrors() override
    {
        noteEvents();
        _overflow = false;
        _dataError = false;
    }

private:
    void noteEvents() const
    {
        if (_events == nullptr)
            return;
        uart_event_t event{};
        while (xQueueReceive(_events, &event, 0) == pdTRUE) {
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL)
                _overflow = true;
            else if (event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR)
                _dataError = true;
        }
    }

    [[nodiscard]] bool configure(uint32_t baud_hz, const Frame& fmt) noexcept
    {
        uart_parity_t parity = UART_PARITY_DISABLE;
        if (fmt.parity == Parity::Even)
            parity = UART_PARITY_EVEN;
        else if (fmt.parity == Parity::Odd)
            parity = UART_PARITY_ODD;

        uart_config_t cfg = {};
        cfg.baud_rate = static_cast<int>(baud_hz);
        cfg.data_bits = UART_DATA_8_BITS;
        cfg.parity = parity;
        cfg.stop_bits = (fmt.stop_bits == StopBits::Two) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
        cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        cfg.source_clk = UART_SCLK_DEFAULT;

        if (uart_param_config(N, &cfg) != ESP_OK)
            return false;
        if (_havePins && uart_set_pin(N, _txPin, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
            return false;
        return true;
    }
};

} // namespace Usart
