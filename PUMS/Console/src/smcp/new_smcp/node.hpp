/**
 * @file new_smcp/node.hpp
 * @brief smcp::Node — узел SMCP поверх comm::Node.
 *
 * Обратная связь узловых протоколов — protected-методы. Зовут их только friend.
 * onState / onError уже виртуальные у comm::Node.
 * Потомок переопределяет нужные. Пустая реализация — событие можно не слушать.
 */

#pragma once

#include "comm/node.hpp"
#include "smcp/new_smcp/packet.hpp"
#include "smcp/new_smcp/pdu.hpp"

namespace smcp {

namespace proto {
class FaultProto;
} // namespace proto

class Node : public comm::Node<Packet> {
    friend class proto::FaultProto;

public:
    explicit Node(comm::ILink<Packet>& link) noexcept
        : comm::Node<Packet>(link)
    {}

protected:
    /** Входящий Fault. Кадр широковещательный, сессия его не видит. */
    virtual void onFault(addr_t src, const Fault& body) noexcept
    {
        (void)src;
        (void)body;
    }
};

} // namespace smcp
