#include "rs422_uart.h"

#include "board_eth01.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "rs422";

size_t Rs422Uart::write(const uint8_t *data, size_t size) {
    if (!open_ || data == nullptr || size == 0) {
        return 0;
    }
    const int n = uart_write_bytes(static_cast<uart_port_t>(RS422_UART_NUM), data, size);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

size_t Rs422Uart::read(uint8_t *buffer, size_t maxSize) {
    if (!open_ || buffer == nullptr || maxSize == 0) {
        return 0;
    }
    const int n = uart_read_bytes(static_cast<uart_port_t>(RS422_UART_NUM), buffer, maxSize, 0);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

size_t Rs422Uart::available() const {
    if (!open_) {
        return 0;
    }
    size_t n = 0;
    (void)uart_get_buffered_data_len(static_cast<uart_port_t>(RS422_UART_NUM), &n);
    return n;
}

size_t Rs422Uart::availableForWrite() const {
    return open_ ? 256 : 0;
}

void Rs422Uart::purge() {
    if (open_) {
        (void)uart_flush_input(static_cast<uart_port_t>(RS422_UART_NUM));
    }
}

void Rs422Uart::purgeOutput() {}

void Rs422Uart::flush() {
    if (open_) {
        (void)uart_wait_tx_done(static_cast<uart_port_t>(RS422_UART_NUM), pdMS_TO_TICKS(100));
    }
}

bool Rs422Uart::open(uint32_t baud) {
    if (baud == 0) {
        return false;
    }
    if (open_) {
        return true;
    }

    uart_config_t cfg = {};
    cfg.baud_rate = static_cast<int>(baud);
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    const uart_port_t port = static_cast<uart_port_t>(RS422_UART_NUM);

    // Сначала пины: дефолт UART2 RX=GPIO16 занят питанием PHY на ETH01.
    if (uart_set_pin(port, PIN_RS422_TX, PIN_RS422_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) !=
        ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed (TX=%d RX=%d)", PIN_RS422_TX, PIN_RS422_RX);
        return false;
    }
    if (uart_param_config(port, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed");
        return false;
    }
    if (uart_driver_install(port, 512, 512, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed on UART%d", RS422_UART_NUM);
        return false;
    }

    open_ = true;
    status_ = Status::OK;
    ESP_LOGI(TAG, "UART%d RS-422 %lu baud TXD=GPIO%d RXD=GPIO%d (debug stays UART0 GPIO1/3)",
             RS422_UART_NUM, static_cast<unsigned long>(baud), PIN_RS422_TX, PIN_RS422_RX);
    return true;
}

void Rs422Uart::close() {
    if (!open_) {
        return;
    }
    (void)uart_driver_delete(static_cast<uart_port_t>(RS422_UART_NUM));
    open_ = false;
}

bool Rs422Uart::isOpen() {
    return open_;
}

BIF::IByteStream::Status Rs422Uart::getStatus() {
    return open_ ? status_ : Status::OK;
}

void Rs422Uart::clearErrors() {
    status_ = Status::OK;
}
