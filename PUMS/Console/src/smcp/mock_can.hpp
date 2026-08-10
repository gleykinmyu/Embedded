/**
 * @file mock_can.hpp
 * @brief Тестовая заглушка BIF::CAN::ICAN: два порта соединяются peer-to-peer.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ican.hpp"

namespace smcp {

class MockCan : public BIF::CAN::ICAN {
public:
    static constexpr std::size_t kRxDepth = 32u;

    MockCan() noexcept = default;

    /** Двусторонняя связь (A.send → B.recv и наоборот). */
    void connect(MockCan& peer) noexcept;

    bool open(uint32_t bitrate) override;
    void close() override;
    bool isOpen() override;

    bool send(const BIF::CAN::Frame& frame) override;
    bool recv(BIF::CAN::Frame& out) override;

    std::size_t available() const override;
    std::size_t availableForWrite() const override;

    void purge() override;
    void purgeOutput() override;
    void flush() override;

    BIF::CAN::Status getStatus() override;
    void clearErrors() override;

private:
    [[nodiscard]] bool pushRx(const BIF::CAN::Frame& frame) noexcept;

    MockCan* _peer = nullptr;
    bool _open = false;
    BIF::CAN::Status _status = BIF::CAN::Status::OK;
    BIF::CAN::Frame _rx[kRxDepth]{};
    std::size_t _head = 0;
    std::size_t _tail = 0;
    std::size_t _count = 0;
};

} // namespace smcp
