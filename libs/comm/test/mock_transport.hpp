/**
 * @file comm/test/mock_transport.hpp
 * @brief Тестовый транспорт: внутренние слоты, буфер кадра и ручное завершение TX.
 *
 * Слоты — деталь мока. Общий Transport видит только txSpace, transmit, pollTx и receive.
 * transmit не завершает передачу. Итог задаёт complete() или таймаут слота.
 * Успешный transmit пишет TxJob* в TxSlot. pollTx обнуляет его до onTxDone.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bitsery/custom/msb_bitpack.hpp"
#include "bitsery/custom/serializer.hpp"
#include "comm/transport.hpp"
#include "comm/tx_slot.hpp"
#include "ms_timer.hpp"

namespace comm {

template <uint8_t Slots = 1>
class MockTransport : public Transport<MockTransport<Slots>> {
    static_assert(Slots >= 1u && Slots < 255u, "MockTransport: slots");
    friend class Transport<MockTransport<Slots>>;

public:
    static constexpr uint8_t kCap = 16;
    static constexpr uint8_t kRxDepth = 8;
    static constexpr uint8_t kRxPerTick = 2;

    MockTransport() = default;
    ~MockTransport() { this->close(); }

    void setTimeout(MISC::time_t ms) noexcept { _timeoutMs = ms; }
    void refuseNext() noexcept { _refuse = true; }

    [[nodiscard]] bool complete(uint8_t slot, bool ok) noexcept
    {
        if (slot >= Slots) {
            return false;
        }
        Slot& s = _slots[slot];
        if (!s.used || s.pending) {
            return false;
        }
        s.pending = true;
        s.ok = ok;
        return true;
    }

    [[nodiscard]] bool inject(const uint8_t* data, size_t len) noexcept
    {
        if (data == nullptr || len > kCap || _rxCount == kRxDepth) {
            return false;
        }
        Rx& rx = _rx[_rxTail];
        rx.len = static_cast<uint8_t>(len);
        if (len != 0u) {
            std::memcpy(rx.data, data, len);
        }
        _rxTail = static_cast<uint8_t>((_rxTail + 1u) % kRxDepth);
        ++_rxCount;
        return true;
    }

    [[nodiscard]] uint8_t rxPending() const noexcept { return _rxCount; }
    [[nodiscard]] size_t size(uint8_t slot) const noexcept { return _slots[slot].len; }
    [[nodiscard]] size_t bits(uint8_t slot) const noexcept { return _slots[slot].bits; }
    [[nodiscard]] const uint8_t* data(uint8_t slot) const noexcept { return _slots[slot].buf; }
    [[nodiscard]] TxJob* slotJob(uint8_t slot) const noexcept { return _slots[slot].job; }

    [[nodiscard]] bool holds(const TxJob& job) const noexcept
    {
        for (uint8_t i = 0; i < Slots; ++i) {
            if (_slots[i].job == &job) {
                return true;
            }
        }
        return false;
    }

private:
    struct Slot : TxSlot {
        bool used = false;
        bool pending = false;
        bool ok = false;
        uint8_t buf[kCap]{};
        size_t len = 0;
        size_t bits = 0;
        MISC::MsTimer timer{};
    };

    struct Rx {
        uint8_t len = 0;
        uint8_t data[kCap]{};
    };

    void onTick(MISC::time_t now) noexcept { _now = now; }

    [[nodiscard]] size_t txSpace() const noexcept
    {
        size_t free = 0;
        for (uint8_t i = 0; i < Slots; ++i) {
            if (!_slots[i].used) {
                ++free;
            }
        }
        return free;
    }

    template <typename Frame>
    bool transmit(TxJob& job, const Frame& frame) noexcept
    {
        uint8_t slot = Slots;
        for (uint8_t i = 0; i < Slots; ++i) {
            if (!_slots[i].used) {
                slot = i;
                break;
            }
        }
        if (slot == Slots) {
            return false;
        }
        bitsery::custom::Measure measured{bitsery::custom::msb::MeasureAdapter<>{}};
        measured.object(frame);
        const auto& measure = measured.adapter();
        if (measure.error() != bitsery::custom::msb::WriteError::NoError
            || measure.writtenBytesCount() > kCap) {
            return false;
        }
        if (_refuse) {
            _refuse = false;
            return false;
        }
        uint8_t raw[kCap]{};
        bitsery::custom::Writer writer{bitsery::custom::msb::OutputAdapter<>{raw, kCap}};
        writer.object(frame);
        const auto& out = writer.adapter();
        if (out.error() != bitsery::custom::msb::WriteError::NoError) {
            return false;
        }
        Slot& dst = _slots[slot];
        dst = Slot{};
        std::memcpy(dst.buf, raw, out.writtenBytesCount());
        dst.len = out.writtenBytesCount();
        dst.bits = out.bitPos();
        dst.job = &job;
        dst.used = true;
        if (_timeoutMs != 0u) {
            dst.timer.start(_now, _timeoutMs);
        }
        return true;
    }

    bool pollTx(TxJob*& job, TxResult& result) noexcept
    {
        for (uint8_t i = 0; i < Slots; ++i) {
            Slot& s = _slots[i];
            if (!s.used) {
                continue;
            }
            const bool timed = s.timer.timedOut(_now);
            if (!s.pending && !timed) {
                continue;
            }
            job = s.job;
            result = s.pending ? (s.ok ? TxResult::Ok : TxResult::Failed) : TxResult::Timeout;
            s = Slot{};
            return true;
        }
        return false;
    }

    template <typename Fn>
    void receive(Fn&& fn)
    {
        uint8_t budget = kRxPerTick;
        while (budget != 0u && _rxCount != 0u) {
            Rx& rx = _rx[_rxHead];
            bitsery::custom::Reader reader{bitsery::custom::msb::InputAdapter<>{rx.data, rx.len}};
            fn(reader);
            _rxHead = static_cast<uint8_t>((_rxHead + 1u) % kRxDepth);
            --_rxCount;
            --budget;
        }
    }

    /** Кадр без готового итога завершается как Failed. Уже записанный итог не меняется. */
    void shutdown() noexcept
    {
        for (uint8_t i = 0; i < Slots; ++i) {
            Slot& s = _slots[i];
            if (s.used && !s.pending) {
                s.pending = true;
                s.ok = false;
            }
        }
    }

    Slot _slots[Slots]{};
    Rx _rx[kRxDepth]{};
    uint8_t _rxHead = 0;
    uint8_t _rxTail = 0;
    uint8_t _rxCount = 0;
    MISC::time_t _now = 0;
    MISC::time_t _timeoutMs = 0;
    bool _refuse = false;
};

} // namespace comm
