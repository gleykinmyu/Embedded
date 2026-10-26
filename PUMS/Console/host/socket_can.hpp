/**
 * @file socket_can.hpp
 * @brief ICAN поверх TCP localhost: два процесса обмениваются Frame.
 *
 * Wire: фиксированная запись 13 байт — id.pack() LE + dlc + data[8].
 * Роль listen (server) / connect (console). Неблокирующий poll в recv/send.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ican.hpp"
#include "ringbuffer.hpp"

namespace smcp {
namespace host {

class SocketCan : public BIF::CAN::ICAN {
public:
    static constexpr std::size_t kRxDepth = 64u;
    static constexpr uint16_t kDefaultPort = 19740u;

    enum class Role : uint8_t { Listen = 0, Connect };

    SocketCan() noexcept = default;
    ~SocketCan() override;

    SocketCan(const SocketCan&) = delete;
    SocketCan& operator=(const SocketCan&) = delete;

    /**
     * @param role Listen — bind/accept; Connect — connect с retry.
     * @param port TCP 127.0.0.1:@a port
     */
    bool configure(Role role, uint16_t port = kDefaultPort) noexcept;

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

    /** Подтянуть RX с сокета (вызывается из recv/available; можно из цикла). */
    void poll() noexcept;

    /** Инъекция тишины: TX «успешен», но в шину не уходит; RX с сокета сливается. */
    void setMute(bool tx, bool rx) noexcept
    {
        _mute_tx = tx;
        _mute_rx = rx;
    }
    [[nodiscard]] bool muteTx() const noexcept { return _mute_tx; }
    [[nodiscard]] bool muteRx() const noexcept { return _mute_rx; }

private:
    static constexpr std::size_t kWireSize = 13u;

    [[nodiscard]] bool ensurePeer() noexcept;
    [[nodiscard]] bool writeAll(const uint8_t* data, std::size_t n) noexcept;
    void closeFds() noexcept;
    [[nodiscard]] static bool packWire(const BIF::CAN::Frame& frame, uint8_t out[kWireSize]) noexcept;
    [[nodiscard]] static bool unpackWire(const uint8_t in[kWireSize], BIF::CAN::Frame& out) noexcept;

    Role _role = Role::Listen;
    uint16_t _port = kDefaultPort;
    int _listen_fd = -1;
    int _fd = -1;          /**< Установленный peer. */
    int _connecting_fd = -1; /**< Connect in progress (non-blocking). */
    bool _open = false;
    bool _configured = false;
    bool _mute_tx = false;
    bool _mute_rx = false;
    BIF::CAN::Status _status = BIF::CAN::Status::OK;
    MISC::RingBuffer<BIF::CAN::Frame, kRxDepth> _rx;
    uint8_t _partial[kWireSize]{};
    std::size_t _partial_n = 0;
};

} // namespace host
} // namespace smcp
