/**
 * @file new_smcp/can_link.hpp
 * @brief comm::ILink<Frame> над BIF::CAN::ICAN.
 *
 * CAN ID (29 bit): msg[28:22] | dst[21:14] | src[13:6] | txn[5:0]
 * payload[0..7] — тело Message (txn в ID, не в payload).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ican.hpp"
#include "smcp/new_smcp/transport.hpp"

namespace smcp {

inline constexpr unsigned kCanIdTxnPos = 0;
inline constexpr unsigned kCanIdSrcPos = 6;
inline constexpr unsigned kCanIdDstPos = 14;
inline constexpr unsigned kCanIdMsgPos = 22;

class CanLink : public comm::ILink<Frame> {
public:
    static constexpr uint8_t kTxCap = 8u;

    explicit CanLink(BIF::CAN::ICAN& can) noexcept
        : _can(&can)
    {}

    [[nodiscard]] State state() const noexcept override
    {
        if (!_can->isOpen()) {
            return State::Closed;
        }
        if (_fault || _can->getStatus() == BIF::CAN::Status::BusOff) {
            return State::Fault;
        }
        return State::Open;
    }

    void clearFault() noexcept override
    {
        _fault = false;
        _can->clearErrors();
    }

    [[nodiscard]] std::size_t txSpace() const noexcept override
    {
        if (state() != State::Open) {
            return 0u;
        }
        const std::size_t slots = static_cast<std::size_t>(kTxCap - _txN);
        const std::size_t wire = _can->availableForWrite();
        return slots < wire ? slots : wire;
    }

    [[nodiscard]] std::size_t txBusy() const noexcept override { return _txN; }

    [[nodiscard]] bool send(const Frame& pkt, void* cookie) noexcept override
    {
        if (state() != State::Open || _txN >= kTxCap) {
            return false;
        }
        BIF::CAN::Frame wire{};
        if (!encode(pkt, wire) || !_can->send(wire)) {
            return false;
        }
        for (uint8_t i = 0; i < kTxCap; ++i) {
            if (_tx[i].busy) {
                continue;
            }
            _tx[i].pkt = pkt;
            _tx[i].cookie = cookie;
            _tx[i].res = TxResult::Ok;
            _tx[i].busy = true;
            _tx[i].done = true;
            ++_txN;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool pollTx(Frame& pkt, void*& cookie, TxResult& result) noexcept override
    {
        for (uint8_t i = 0; i < kTxCap; ++i) {
            if (!_tx[i].busy || !_tx[i].done) {
                continue;
            }
            pkt = _tx[i].pkt;
            cookie = _tx[i].cookie;
            result = _tx[i].res;
            _tx[i] = {};
            --_txN;
            return true;
        }
        return false;
    }

    void abortTx() noexcept override
    {
        for (uint8_t i = 0; i < kTxCap; ++i) {
            if (_tx[i].busy) {
                _tx[i].res = TxResult::Failed;
                _tx[i].done = true;
            }
        }
        _can->purgeOutput();
    }

    [[nodiscard]] bool receive(Frame& out) noexcept override
    {
        if (state() != State::Open) {
            return false;
        }
        BIF::CAN::Frame wire{};
        if (!_can->recv(wire)) {
            return false;
        }
        if (!decode(wire, out)) {
            return false;
        }
        return true;
    }

private:
    struct Slot {
        Frame pkt{};
        void* cookie = nullptr;
        TxResult res = TxResult::Ok;
        bool busy = false;
        bool done = false;
    };

    [[nodiscard]] static bool encode(const Frame& pkt, BIF::CAN::Frame& wire) noexcept
    {
        if (pkt.payload.cursor() > Message::kMaxLen) {
            return false;
        }
        const uint32_t mid = static_cast<uint32_t>(static_cast<uint8_t>(pkt.id) & kMsgIdMax);
        const uint32_t d = static_cast<uint32_t>(pkt.dst.v);
        const uint32_t s = static_cast<uint32_t>(pkt.src.v);
        const uint32_t t = static_cast<uint32_t>(pkt.txn & kTxnIdMax);
        const uint32_t can_id = (mid << kCanIdMsgPos) | (d << kCanIdDstPos)
            | (s << kCanIdSrcPos) | (t << kCanIdTxnPos);

        wire = {};
        wire.id.setExtended(true);
        wire.id.set(can_id);
        wire.dlc = static_cast<uint8_t>(pkt.payload.cursor());
        if (pkt.payload.cursor() > 0u) {
            std::memcpy(wire.data, pkt.payload.data(), pkt.payload.cursor());
        }
        return true;
    }

    [[nodiscard]] static bool decode(const BIF::CAN::Frame& wire, Frame& pkt) noexcept
    {
        if (!wire.id.extended || wire.dlc > BIF::CAN::kMaxDataLength) {
            return false;
        }
        const uint32_t can_id = wire.id.get();
        pkt = {};
        pkt.id = static_cast<MsgId>((can_id >> kCanIdMsgPos) & kMsgIdMax);
        pkt.dst.v = static_cast<uint8_t>((can_id >> kCanIdDstPos) & 0xFFu);
        pkt.src.v = static_cast<uint8_t>((can_id >> kCanIdSrcPos) & 0xFFu);
        pkt.txn = static_cast<uint8_t>((can_id >> kCanIdTxnPos) & kTxnIdMax);
        (void)pkt.payload.push(wire.data, wire.dlc);
        return true;
    }

    BIF::CAN::ICAN* _can;
    Slot _tx[kTxCap]{};
    uint8_t _txN = 0;
    bool _fault = false;
};

} // namespace smcp
