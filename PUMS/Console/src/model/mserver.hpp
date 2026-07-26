/**
 * @file mserver.hpp
 * @brief Модель сервера сегмента: Select → mechs, Telemetry → TX-очередь.
 *
 * Пока без CAN: входящие пакеты кладут в rx(), poll() обрабатывает Select
 * и ставит Telemetry в tx(). Консоль/loopback читает tx().
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "model/drive_mech.hpp"
#include "smcp/message.hpp"
#include "smcp/packet_queue.hpp"

class MServer {
public:
    static constexpr std::size_t kRxDepth = 16u;
    static constexpr std::size_t kTxDepth = 32u;

    explicit MServer(uint8_t server_id = smcp::msg::kServerIdMin) noexcept;

    [[nodiscard]] uint8_t id() const noexcept { return _server_id; }

    [[nodiscard]] smcp::msg::PacketQueue<kRxDepth>& rx() noexcept { return _rx; }
    [[nodiscard]] smcp::msg::PacketQueue<kTxDepth>& tx() noexcept { return _tx; }
    [[nodiscard]] const smcp::msg::PacketQueue<kRxDepth>& rx() const noexcept { return _rx; }
    [[nodiscard]] const smcp::msg::PacketQueue<kTxDepth>& tx() const noexcept { return _tx; }

    [[nodiscard]] DriveMech& mech(uint8_t mech_id) noexcept;
    [[nodiscard]] const DriveMech& mech(uint8_t mech_id) const noexcept;

    /**
     * Разобрать все пакеты из rx (Select), обновить mechs,
     * при изменениях положить Telemetry в tx.
     */
    void poll() noexcept;

    /** Положить Telemetry по одной оси в tx (если есть место). */
    [[nodiscard]] bool pushTelemetry(uint8_t mech_id) noexcept;

private:
    void handlePacket(const smcp::msg::Packet& pkt) noexcept;
    void handleSelect(const smcp::msg::Header& hdr, const smcp::msg::Select& body) noexcept;

    [[nodiscard]] uint8_t nextPktId() noexcept { return _pkt_tx++; }

    uint8_t _server_id;
    uint8_t _pkt_tx = 0;
    DriveMech _mechs[smcp::kMechCount];
    smcp::msg::PacketQueue<kRxDepth> _rx;
    smcp::msg::PacketQueue<kTxDepth> _tx;
};
