/**
 * @file link.hpp
 * @brief SMCP Gateway: BIF::CAN::ICAN ↔ smcp::msg::Packet.
 *
 * Только транспорт. Heartbeat / peer / pkt_id-политика — у Session (см. PROTOCOL.md).
 * TX: Message + dst (+ опц. pkt_id); src/prio/msg_id заполняет Link.
 */

#pragma once

#include <cstdint>

#include "ican.hpp"
#include "smcp/transport/message.hpp"

namespace smcp {

class Link {
public:
    enum class Status : uint8_t {
        OK = 0,
        EncodeFailed,
        DecodeFailed,
        CanSendFailed,
        CanClosed,
    };

    explicit Link(BIF::CAN::ICAN& can, uint8_t node_id = 1u) noexcept;

    [[nodiscard]] uint8_t nodeId() const noexcept { return _node_id; }
    [[nodiscard]] BIF::CAN::ICAN& can() noexcept { return _can; }
    [[nodiscard]] const BIF::CAN::ICAN& can() const noexcept { return _can; }

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::OK; }

    /**
     * Unicast / broadcast: src=nodeId, prio/msg_id из body.
     * Broadcast: dst_id = msg::kBroadcastId.
     */
    [[nodiscard]] bool send(const msg::Message& body,
                            uint8_t dst_id = msg::kBroadcastId,
                            uint8_t pkt_id = 0) noexcept;

    /**
     * Один кадр ICAN::recv → Packet.
     * @return false — нет кадра / ошибка разбора.
     */
    [[nodiscard]] bool receive(msg::Packet& out) noexcept;

private:
    BIF::CAN::ICAN& _can;
    uint8_t _node_id;
    Status _status = Status::OK;
};

} // namespace smcp
