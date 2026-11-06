/**
 * @file session.hpp
 * @brief Сессия к одному peer: фасад FSM (PROTOCOL.md).
 *
 * session::LinkHb | session::ReplyWnd | session::TaggedOps | session::MultiByte | _tx_ctrl.
 * session::MultiByte TX: сам peek→Session::send; Session::onTxResult → commit.
 */

#pragma once

#include <cstdint>

#include "mem/polyslot.hpp"
#include "smcp/node.hpp"
#include "smcp/session/link_hb.hpp"
#include "smcp/session/multi_byte.hpp"
#include "smcp/session/reply_wnd.hpp"
#include "smcp/session/tagged_ops.hpp"

namespace smcp {

#ifndef SMCP_SESSION_CTRL_TX_CAPACITY
#define SMCP_SESSION_CTRL_TX_CAPACITY 8u
#endif

class Session {
public:
    using Status = session::LinkHb::Status;
    using Link = Node::Link;

    [[nodiscard]] static const char* cstr(Status status) noexcept
    {
        return session::LinkHb::cstr(status);
    }
    [[nodiscard]] static const char* cstr(Link link) noexcept { return Node::cstr(link); }
    [[nodiscard]] static const char* cstr(session::MultiByte::Error status) noexcept
    {
        return session::MultiByte::cstr(status);
    }
    [[nodiscard]] static const char* cstr(session::TaggedOps::Result status) noexcept
    {
        return session::TaggedOps::cstr(status);
    }

    virtual ~Session() = default;
    explicit Session(Node& node) noexcept;

    Node& node;

    [[nodiscard]] Status getStatus() const noexcept { return _link.status(); }
    [[nodiscard]] uint8_t peerId() const noexcept { return _peer_id; }
    [[nodiscard]] bool isOpen() const noexcept { return _link.isOpen(); }
    [[nodiscard]] bool isMaster() const noexcept { return _master; }

    void open(uint8_t peer_id) noexcept;
    void start(uint8_t peer_id) noexcept;
    /** @a link ≠ Up (Up → Down). Down — штатно; HbLost/PeerDown — lost. */
    void close(Link link = Link::Down) noexcept;

    /**
     * Short RPC → session::ReplyWnd (ожидание Ack|Nack).
     * @return true — в очереди.
     */
    template <typename T>
    bool request(const T& pdu) noexcept
    {
        if (!isOpen()) {
            return false;
        }
        Message m{};
        if (!pdu.pack(m)) {
            node.onPackFailed(this, T::kId);
            return false;
        }
        return enqueueReq(m, allocTxn());
    }

    template <typename T>
    bool request(const T& pdu, uint8_t txn_id) noexcept
    {
        if (!isOpen() || txn_id == 0u) {
            return false;
        }
        Message m{};
        if (!pdu.pack(m)) {
            node.onPackFailed(this, T::kId);
            return false;
        }
        return enqueueReq(m, txn_id);
    }

    /**
     * Unicast в `_tx_ctrl` (без окна Ack).
     * HB / Ack / Nack / тело Get-Set / notify.
     */
    bool send(Message msg, uint8_t txn_id = 0) noexcept;

    /** Pack @a pdu в @a out, затем send(out, txn). onPackFailed при сбое pack. */
    template <typename T>
    bool send(const T& pdu, Message& out, uint8_t txn_id = 0) noexcept
    {
        if (!pdu.pack(out)) {
            node.onPackFailed(this, T::kId);
            return false;
        }
        return send(out, txn_id);
    }

    template <typename T>
    bool send(const T& pdu, uint8_t txn_id = 0) noexcept
    {
        Message m{};
        return send(pdu, m, txn_id);
    }

    /** Ответ на Get: тело с echo txn (session::TaggedOps). index — из opener. */
    [[nodiscard]] bool send(uint8_t tag, const IBuf& buf, uint8_t txn_id,
                            uint8_t index = session::kIndexAll) noexcept
    {
        return _tagged.sendBody(tag, buf, txn_id, index);
    }

    template <typename Tag>
    [[nodiscard]] bool send(Tag tag, const IBuf& buf, uint8_t txn_id,
                            uint8_t index = session::kIndexAll) noexcept
    {
        return send(static_cast<uint8_t>(tag), buf, txn_id, index);
    }

    [[nodiscard]] uint8_t txnId() const noexcept { return _txn_tx; }

    void sendAck(uint8_t txn_id) noexcept;
    void sendNack(uint8_t txn_id, session::vError error,
                  uint32_t detail = session::Nack::kDetailNone) noexcept;

    /** Хук Node: кадр или локальный Timeout (FSM). */
    inline void ack(const TxSlot& req, const session::Ack& reply) noexcept
    {
        node.onAck(this, req, reply);
    }

    inline void nack(const TxSlot& req, const session::Nack& reply) noexcept
    {
        node.onNack(this, req, reply);
    }
    inline void nack(const TxSlot& req, session::vError error,
                     uint32_t detail = session::Nack::kDetailNone) noexcept
    {
        nack(req, session::Nack::make(error, detail));
    }

    /** @a index = @c session::kIndexAll — весь объект; иначе один экземпляр (ось). */
    [[nodiscard]] inline session::TaggedOps::Result set(uint8_t tag, const IBuf& buf,
                                         uint8_t index = session::kIndexAll) noexcept
    {
        return _tagged.set(tag, buf, index);
    }
    [[nodiscard]] inline session::TaggedOps::Result get(uint8_t tag, IBuf& buf,
                                         uint8_t index = session::kIndexAll) noexcept
    {
        return _tagged.get(tag, buf, index);
    }

protected:
    [[nodiscard]] virtual bool onPacket(const Packet& pkt) noexcept;
    virtual void tick() noexcept;

    virtual void onGet(const session::Get& body, uint8_t txn_id) noexcept;
    [[nodiscard]] virtual bool onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                                           uint16_t size, IBuf& buf) noexcept;
    /**
     * Set после успешного транспорта. None → Ack; иначе Nack(error).
     * При сбое тела session::TaggedOps шлёт Body и сюда не заходит.
     */
    [[nodiscard]] virtual session::vError onSet(uint8_t tag, uint8_t index,
                                            const IBuf& buf) noexcept;
    virtual void onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                         session::MultiByte::Error status) noexcept;

    session::ReplyWnd _reply;
    session::LinkHb _link;
    session::MultiByte _mb;
    session::TaggedOps _tagged;

private:
    friend class Node;
    friend class session::TaggedOps;

    [[nodiscard]] const TxSlot* peekTx() noexcept;
    void onTxResult(bool ok) noexcept;

    enum class OutSrc : uint8_t { None, Ctrl, Req };

    [[nodiscard]] uint8_t allocTxn() noexcept;
    [[nodiscard]] bool enqueueReq(Message msg, uint8_t txn_id) noexcept;
    void clearTx() noexcept;
    [[nodiscard]] const TxSlot* peekCtrl() noexcept;

    uint8_t _peer_id = 0;
    uint8_t _txn_tx = 0;

    bool _master = false;
    OutSrc _out = OutSrc::None;
    bool _rr_ctrl = true;

    TxQueue<SMCP_SESSION_CTRL_TX_CAPACITY> _tx_ctrl;
};

/**
 * Плотный пул Session[N] = PolyStore&lt;Session, S&gt;.
 * Leaf: `SessionBank&lt;N, ShowSession&gt; bank(*this);` + `sessions() { return bank; }`.
 * Индекс слота — только у Node::session(i) / forEach; на Session не хранится.
 */
template <uint8_t N, typename S = Session>
using SessionBank = mem::PolyStore8<Session, S, N>;

} // namespace smcp
