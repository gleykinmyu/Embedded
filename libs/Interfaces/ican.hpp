#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ilockable.hpp"

namespace BIF {
namespace CAN {

// =================================================================
// Кадр (общий для интерфейса и драйверов)
// Упаковка Id::pack/unpack — раскладка bxCAN TIxR/RIxR (как STM32 CAN_TI0R_*).
// =================================================================

namespace bits {
inline constexpr uint32_t kRtr = 1u << 1;
inline constexpr uint32_t kIde = 1u << 2;
inline constexpr unsigned kExidPos = 3;
inline constexpr uint32_t kExidMsk = 0x3FFFFu << 3;
inline constexpr unsigned kStidPos = 21;
inline constexpr uint32_t kStidMsk = 0x7FFu << 21;
} // namespace bits

inline constexpr uint8_t kMaxDataLength = 8;

struct Frame {
    struct Id {
        bool extended = false; ///< IDE.
        bool remote = false;   ///< RTR.

        Id() = default;

        Id(uint32_t v, bool ext = false, bool rtr = false) noexcept
            : extended(ext), remote(rtr)
        {
            set(v);
        }

        [[nodiscard]] uint32_t get() const noexcept { return _value & idMask(); }

        void set(uint32_t v) noexcept { _value = v & idMask(); }

        void setExtended(bool ext) noexcept
        {
            extended = ext;
            _value &= idMask();
        }

        [[nodiscard]] uint32_t pack() const noexcept
        {
            const uint32_t id = get();
            uint32_t v = 0U;
            if (extended) {
                v = ((id << bits::kExidPos) & (bits::kExidMsk | bits::kStidMsk)) | bits::kIde;
            } else {
                v = (id << bits::kStidPos) & bits::kStidMsk;
            }
            if (remote)
                v |= bits::kRtr;
            return v;
        }

        [[nodiscard]] static Id unpack(uint32_t fr) noexcept
        {
            Id out{};
            out.extended = (fr & bits::kIde) != 0U;
            out.remote = (fr & bits::kRtr) != 0U;
            if (out.extended)
                out.set((fr & (bits::kExidMsk | bits::kStidMsk)) >> bits::kExidPos);
            else
                out.set((fr & bits::kStidMsk) >> bits::kStidPos);
            return out;
        }

    private:
        uint32_t _value = 0;

        [[nodiscard]] uint32_t idMask() const noexcept
        {
            return extended ? 0x1FFFFFFFu : 0x7FFu;
        }
    };

    Id id{};
    uint8_t dlc = 0;
    uint8_t data[kMaxDataLength] = {};
};

enum class FilterMode : uint8_t {
    Mask = 0,
    List = 1,
};

enum class FilterScale : uint8_t {
    Dual16 = 0,
    Single32 = 1,
};

enum class FilterFifo : uint8_t {
    Fifo0 = 0,
    Fifo1 = 1,
};

enum class Status : uint8_t {
    OK = 0,
    OverFlowRX, ///< SW RX-очередь или HW FIFO overrun.
    BusOff,
    ErrorPassive,
    ErrorWarning,
};

inline const char* cstr(Status s) noexcept
{
    switch (s) {
    case Status::OK: return "OK";
    case Status::OverFlowRX: return "OverFlowRX";
    case Status::BusOff: return "BusOff";
    case Status::ErrorPassive: return "ErrorPassive";
    case Status::ErrorWarning: return "ErrorWarning";
    default: return "?";
    }
}

// =================================================================
// ICAN — публичный контракт (приложение / протоколы)
// Без IRQ и без регистров: только кадры и lifecycle.
// =================================================================

class ICAN
{
public:
    virtual ~ICAN() = default;

    virtual bool open(uint32_t bitrate) = 0;
    virtual void close() = 0;
    virtual bool isOpen() = 0;

    virtual bool send(const Frame& frame) = 0;
    virtual bool recv(Frame& out) = 0;

    virtual size_t available() const = 0;
    virtual size_t availableForWrite() const = 0;

    virtual void purge() = 0;
    virtual void purgeOutput() = 0;
    virtual void flush() = 0;

    virtual Status getStatus() = 0;
    virtual void clearErrors() = 0;
};

// =================================================================
// IHardwareCAN — железо + IRQ (для драйвера платы / VTOR)
// Наследует ICAN; IRQ_* вешаются на векторы; try*Hardware — в subclass.
// =================================================================

class IHardwareCAN : public ICAN, public ILockable
{
public:
    virtual void IRQ_TX_Handler() = 0;
    virtual void IRQ_RX0_Handler() = 0;
    virtual void IRQ_RX1_Handler() = 0;
    virtual void IRQ_SCE_Handler() = 0;

protected:
    virtual void IRQ_TX_Enable() = 0;
    virtual void IRQ_TX_Disable() = 0;
    virtual void IRQ_RX_Enable() = 0;
    virtual void IRQ_RX_Disable() = 0;

    virtual bool tryTransmitHardware(const Frame& frame) = 0;
    virtual bool tryReceiveHardware(Frame& out) = 0;

    virtual bool isHardwareTxBusy() const = 0;
    virtual void checkErrors() = 0;
};

template <typename T, size_t Size>
class ObjectRingBuffer
{
    static_assert(Size >= 2, "ObjectRingBuffer: Size >= 2");

    T _buffer[Size];
    volatile size_t _head = 0;
    volatile size_t _tail = 0;
    volatile size_t _ovfCount = 0;

public:
    bool push(const T& item) noexcept
    {
        const size_t next = (_head + 1) % Size;
        if (next == _tail) {
            ++_ovfCount;
            return false;
        }
        _buffer[_head] = item;
        _head = next;
        return true;
    }

    bool pop(T& item) noexcept
    {
        if (_head == _tail)
            return false;
        item = _buffer[_tail];
        _tail = (_tail + 1) % Size;
        return true;
    }

    [[nodiscard]] const T* peek() const noexcept
    {
        if (_head == _tail)
            return nullptr;
        return &_buffer[_tail];
    }

    void drop() noexcept
    {
        if (_head != _tail)
            _tail = (_tail + 1) % Size;
    }

    size_t size() const noexcept
    {
        return (_head >= _tail) ? (_head - _tail) : (Size - _tail + _head);
    }

    size_t space() const noexcept { return (Size - 1) - size(); }

    void clearData() noexcept { _head = _tail = 0; }

    size_t overflows() const noexcept { return _ovfCount; }
    void clearOverflows() noexcept { _ovfCount = 0; }
};

// =================================================================
// BufferedCAN: SW-очереди + реализация IRQ TX/RX0 поверх IHardwareCAN
// Драйвер платы наследует BufferedCAN и реализует open/close/try*Hardware/…
// Инвариант TX: send() → IRQ_TX_Enable(); IRQ_TX_Handler → tryTransmit + drop.
// =================================================================

template <size_t TxFrames, size_t RxFrames>
class BufferedCAN : public IHardwareCAN
{
    ObjectRingBuffer<Frame, TxFrames> _txQ;
    ObjectRingBuffer<Frame, RxFrames> _rxQ;

protected:
    volatile bool _isOpen = false;
    volatile Status _status = Status::OK;
    volatile bool _hwOverrunRx = false;

public:
    bool isOpen() override { return _isOpen; }

    Status getStatus() override
    {
        if (!_isOpen)
            return Status::OK;
        if (_rxQ.overflows() > 0 || _hwOverrunRx)
            return Status::OverFlowRX;
        return _status;
    }

    void clearErrors() override
    {
        _status = Status::OK;
        _hwOverrunRx = false;
        _rxQ.clearOverflows();
        _txQ.clearOverflows();
    }

    bool send(const Frame& frame) override
    {
        if (!_isOpen)
            return false;
        LockGuard guard(*this);
        if (!_txQ.push(frame))
            return false;
        IRQ_TX_Enable();
        return true;
    }

    bool recv(Frame& out) override
    {
        LockGuard guard(*this);
        return _rxQ.pop(out);
    }

    size_t available() const override { return _rxQ.size(); }
    size_t availableForWrite() const override { return _txQ.space(); }

    void purge() override
    {
        LockGuard guard(*this);
        _rxQ.clearData();
    }

    void purgeOutput() override
    {
        LockGuard guard(*this);
        _txQ.clearData();
        IRQ_TX_Disable();
    }

    void flush() override
    {
        if (!_isOpen)
            return;
        while (_txQ.size() > 0) {
        }
        while (isHardwareTxBusy()) {
        }
    }

    void IRQ_RX0_Handler() override
    {
        checkErrors();
        Frame frame{};
        while (tryReceiveHardware(frame)) {
            if (!_rxQ.push(frame))
                _hwOverrunRx = true;
        }
    }

    void IRQ_RX1_Handler() override {}

    void IRQ_TX_Handler() override
    {
        while (const Frame* frame = _txQ.peek()) {
            if (!tryTransmitHardware(*frame))
                break;
            _txQ.drop();
        }
        if (_txQ.size() == 0)
            IRQ_TX_Disable();
    }

    void IRQ_SCE_Handler() override { checkErrors(); }
};

} // namespace CAN
} // namespace BIF
