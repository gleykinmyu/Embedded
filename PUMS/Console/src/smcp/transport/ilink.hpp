/**
 * @file ilink.hpp
 * @brief Абстракция SMCP-линка: Message/Packet без привязки к среде.
 *
 * send/receive — общая SMCP-логика; среда — isOpen / write / read.
 * Реализации: CanLink (ICAN) и т.п. Heartbeat / peer / pkt_id — у Session.
 */

#pragma once

#include <cstdint>

#include "smcp/transport/message.hpp"

namespace smcp {

class ILink {
public:
    enum class Status : uint8_t {
        OK = 0,
        EncodeFailed,
        DecodeFailed,
        SendFailed,
        Closed,
    };

    virtual ~ILink() = default;

    [[nodiscard]] uint8_t nodeId() const noexcept { return _node_id; }
    void setNodeId(uint8_t node_id) noexcept { _node_id = node_id; }
    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::OK; }

    [[nodiscard]] virtual bool isOpen() noexcept = 0;

    /**
     * Unicast / broadcast: собирает Packet (src/prio/msg_id), затем write().
     * Broadcast: dst_id = msg::kBroadcastId.
     */
    [[nodiscard]] bool send(const msg::Message& body,
                            uint8_t dst_id = msg::kBroadcastId,
                            uint8_t pkt_id = 0) noexcept;

    /**
     * Один входящий кадр → Packet (через read()).
     * @return false — нет кадра / ошибка / закрыт.
     */
    [[nodiscard]] bool receive(msg::Packet& out) noexcept;

protected:
    explicit ILink(uint8_t node_id) noexcept : _node_id(node_id) {}

    /** Среда: Packet → wire. Статус Encode/Send выставляет реализация. */
    [[nodiscard]] virtual bool write(const msg::Packet& pkt) noexcept = 0;

    /**
     * Среда: wire → Packet.
     * @return false — нет кадра / DecodeFailed (статус выставляет реализация).
     */
    [[nodiscard]] virtual bool read(msg::Packet& out) noexcept = 0;

    uint8_t _node_id;
    Status _status = Status::OK;
};

} // namespace smcp
