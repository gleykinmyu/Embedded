#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ibyte_stream.hpp"
#include "ilockable.hpp"
#include "ringbuffer.hpp"

namespace BIF {

// =================================================================
// IByteStream поверх IHWByteStream.
// TX: запись в кольцо включает irqTx; tx-колбэк выгружает байт и при пустом кольце его гасит.
// flush() ждёт пустое кольцо (нужен tx-колбэк) и затем isTxBusy() == false.
// =================================================================

template <size_t TxSize, size_t RxSize>
class ISerial : public IByteStream
{
public:
    explicit ISerial(IHWByteStream& hw) noexcept
        : _hw(hw)
    {
    }

    bool open(uint32_t baud) override
    {
        if (!_hw.open(baud))
            return false;
        _hw.irqRxEnable();
        _isOpen = true;
        return true;
    }

    void close() override
    {
        if (!_isOpen)
            return;
        _hw.irqRxDisable();
        _hw.irqTxDisable();
        _hw.close();
        _isOpen = false;
    }

    bool isOpen() override { return _isOpen; }

    IByteStream::Status getStatus() override
    {
        if (!_isOpen)
            return IByteStream::Status::OK;
        if (_rxBuf.overflows() > 0 || _hwOverrunRx)
            return IByteStream::Status::OverFlowRX;
        if (_dataError)
            return IByteStream::Status::DataError;
        return IByteStream::Status::OK;
    }

    /// Кладёт байты в TX-кольцо; при n > 0 включает TX-прерывание.
    size_t write(const uint8_t* data, size_t size) override
    {
        if (!_isOpen || !data || size == 0)
            return 0;

        LockGuard guard(_hw);
        size_t n = 0;
        while (n < size && _txBuf.push(data[n]))
            ++n;
        if (n > 0)
            _hw.irqTxEnable();
        return n;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override
    {
        if (!buffer)
            return 0;
        LockGuard guard(_hw);
        size_t count = 0;
        while (count < maxSize && _rxBuf.pop(buffer[count]))
            ++count;
        return count;
    }

    size_t available() const override { return _rxBuf.size(); }
    size_t availableForWrite() const override { return _txBuf.space(); }

    void purge() override
    {
        LockGuard guard(_hw);
        _rxBuf.clearData();
    }

    void purgeOutput() override
    {
        LockGuard guard(_hw);
        _txBuf.clearData();
        _hw.irqTxDisable();
    }

    void flush() override
    {
        if (!_isOpen)
            return;
        while (_txBuf.size() > 0) {
        }
        while (_hw.isTxBusy()) {
        }
    }

    void clearErrors() override
    {
        _dataError = false;
        _hwOverrunRx = false;
        _rxBuf.clearOverflows();
        _txBuf.clearOverflows();
    }

protected:
    /// После того как `hw` уже сконструирован (тело конструктора наследника).
    void bindHw() noexcept
    {
        _hw.setRxCallback(&ISerial::rxThunk, this);
        _hw.setTxCallback(&ISerial::txThunk, this);
    }

    void unbindHw() noexcept
    {
        _hw.setRxCallback(nullptr, nullptr);
        _hw.setTxCallback(nullptr, nullptr);
    }

private:
    static void rxThunk(void* ctx) noexcept
    {
        static_cast<ISerial*>(ctx)->onRx();
    }

    static void txThunk(void* ctx) noexcept
    {
        static_cast<ISerial*>(ctx)->onTx();
    }

    void onRx() noexcept
    {
        const IByteStream::Status st = _hw.getStatus();
        const uint8_t byte = _hw.readByte();
        if (st == IByteStream::Status::DataError)
            _dataError = true;
        if (st == IByteStream::Status::OverFlowRX)
            _hwOverrunRx = true;
        if (!_rxBuf.push(byte))
            _hwOverrunRx = true;
    }

    void onTx() noexcept
    {
        uint8_t byte = 0;
        if (_txBuf.pop(byte))
            _hw.writeByte(byte);
        else
            _hw.irqTxDisable();
    }

    IHWByteStream& _hw;
    MISC::RingBuffer<uint8_t, TxSize> _txBuf;
    MISC::RingBuffer<uint8_t, RxSize> _rxBuf;
    volatile bool _isOpen = false;
    volatile bool _dataError = false;
    volatile bool _hwOverrunRx = false;
};

} // namespace BIF
