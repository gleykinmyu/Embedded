/**
 * @file new_smcp/session.hpp
 * @brief smcp::Session — сессия SMCP поверх comm::Session.
 *
 * Обратная связь протоколов — protected-методы. Зовут их только friend-протоколы.
 * Потомок переопределяет нужные и начинает с вызова родителя: базовое тело
 * иначе не выполнится. onAck / onNack / onGet / onSet пустые.
 * onLink при уходе в Idle закрывает сессию. Перезапуск — после родительского вызова.
 *
 * open() скрывает comm::Session::open. true — мастер (ping), false — ведомый.
 */

#pragma once

#include "comm/session.hpp"
#include "smcp/new_smcp/transport.hpp"
#include "smcp/new_smcp/pdu.hpp"
#include "smcp/new_smcp/proto/link_hb.hpp"

namespace smcp {

namespace proto {
class Reply;
class Tagged;
class MultiFrame;
} // namespace proto

class Session : public comm::Session<Frame> {
    friend class proto::LinkHb;
    friend class proto::Reply;
    friend class proto::Tagged;
    friend class proto::MultiFrame;

public:
    using CommSession = comm::Session<Frame>;
    explicit Session(comm::ProtoContainer<Frame>& box) noexcept
        : CommSession(box)
    {}

    /** Peer + LinkHb. master — ping; false — ведомый, Request забирает onPacket. */
    void open(addr_t peer) noexcept
    {
        CommSession::open(peer);
        _hb.start();
    }

protected:
    /**
     * Ребро линка. Наследник вызывает Session::onLink в начале.
     * Уход в Idle с причиной закрывает слот. Перезапуск — open() после этого вызова.
     */
    virtual void onLink(proto::LinkStatus status) noexcept
    {
        if (_hb.isDown()) {
            close();
        }
    }
    
    virtual void onAck(uint8_t txn) noexcept { (void)txn; }
    virtual void onNack(uint8_t txn, Err error, uint32_t detail) noexcept
    {
        (void)txn;
        (void)error;
        (void)detail;
    }
    virtual void onGet(const Get& body, uint8_t txn) noexcept
    {
        (void)body;
        (void)txn;
    }
    virtual void onSet(const Set& body, uint8_t txn) noexcept
    {
        (void)body;
        (void)txn;
    }

private:
    proto::LinkHb _hb{*this};
};

/** Банк без размера. parent() — узел, не голый контейнер. */
class SessionSlots : public comm::SessionSlots<Frame, Session> {
public:
    using NodeT = comm::Node<Frame>;

    [[nodiscard]] NodeT& parent() noexcept override
    {
        return static_cast<NodeT&>(comm::SessionSlots<Frame, Session>::parent());
    }

    [[nodiscard]] const NodeT& parent() const noexcept override
    {
        return static_cast<const NodeT&>(comm::SessionSlots<Frame, Session>::parent());
    }

protected:
    using comm::SessionSlots<Frame, Session>::SessionSlots;
};

} // namespace smcp
