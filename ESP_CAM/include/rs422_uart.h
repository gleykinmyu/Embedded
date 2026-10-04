#pragma once

#include "ibyte_stream.hpp"

#include <cstddef>
#include <cstdint>

/** ESP32 UART1 как BIF::IByteStream → RS-422 (full-duplex). */
class Rs422Uart : public BIF::IByteStream {
public:
    size_t write(const uint8_t *data, size_t size) override;
    size_t read(uint8_t *buffer, size_t maxSize) override;
    size_t available() const override;
    size_t availableForWrite() const override;
    void purge() override;
    void purgeOutput() override;
    void flush() override;
    bool open(uint32_t baud) override;
    void close() override;
    bool isOpen() override;
    Status getStatus() override;
    void clearErrors() override;

private:
    bool open_{};
    Status status_{Status::OK};
};
