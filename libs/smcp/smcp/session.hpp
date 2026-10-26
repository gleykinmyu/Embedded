/**
 * @file session.hpp
 * @brief Сессия к одному peer: фасад FSM (PROTOCOL.md).
 *
 * LinkHb | ReplyWnd | TaggedOps | MultiByte | _tx_ctrl.
 * MultiByte TX: сам peek→Session::send; Session::onTxResult → commit.
 */

#pragma once

#include <cstdint>

#include "obj_bank.hpp"
#include "obj_registry.hpp"
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
    using Status = LinkHb::Status;
    using Fault = Node::Fault;

    [[nodiscard]] static const char* cstr(Status status) noexcept
    {
        return LinkHb::cstr(status);
    }
    [[nodiscard]] static const char* cstr(Fault reason) noexcept { return Node::cstr(reason); }
    [[nodiscard]] static const char* cstr(MultiByte::Error status) noexcept
    {
        return MultiByte::cstr(status);
    }

    virtual ~Session() = default;
    explicit Session(Node& node) noexcept;

    Node& node;

    [[nodiscard]] uint8_t id() const noexcept { return _id; }
    [[nodiscard]] Status getStatus() const noexcept { return _link.status(); }
    [[nodiscard]] uint8_t peerId() const noexcept { return _peer_id; }
    [[nodiscard]] bool isOpen() const noexcept { return _link.isOpen(); }
    [[nodiscard]] bool isMaster() const noexcept { return _master; }

    void open(uint8_t peer_id) noexcept;
    void start(uint8_t peer_id) noexcept;
    void close(Fault reason) noexcept;

    /**
     * Short RPC → ReplyWnd (ожидание Ack|Nack).
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

    /** Ответ на Get: тело с echo txn (TaggedOps). */
    [[nodiscard]] bool send(uint8_t tag, const IBuf& buf, uint8_t txn_id) noexcept
    {
        return _tagged.sendBody(tag, buf, txn_id);
    }

    [[nodiscard]] uint8_t txnId() const noexcept { return _txn_tx; }

    void sendAck(uint8_t txn_id) noexcept;
    void sendNack(uint8_t txn_id, uint8_t error,
                  uint32_t detail = msg::Nack::kDetailNone) noexcept;

    template <typename E>
    void sendNack(uint8_t txn_id, E error,
                  uint32_t detail = msg::Nack::kDetailNone) noexcept
    {
        sendNack(txn_id, static_cast<uint8_t>(error), detail);
    }

    /** Хук Node: кадр или локальный Timeout (FSM). */
    inline void ack(const TxSlot& req, const msg::Ack& reply) noexcept
    {
        node.onAck(this, req, reply);
    }

    inline void nack(const TxSlot& req, const msg::Nack& reply) noexcept
    {
        node.onNack(this, req, reply);
    }
    inline void nack(const TxSlot& req, uint8_t error,
                     uint32_t detail = msg::Nack::kDetailNone) noexcept
    {
        nack(req, msg::Nack::make(error, detail));
    }
    template <typename E>
    void nack(const TxSlot& req, E error,
              uint32_t detail = msg::Nack::kDetailNone) noexcept
    {
        nack(req, static_cast<uint8_t>(error), detail);
    }

    [[nodiscard]] inline bool set(uint8_t tag, const IBuf& buf) noexcept
    {
        return _tagged.set(tag, buf);
    }
    [[nodiscard]] inline bool get(uint8_t tag, IBuf& buf) noexcept
    {
        return _tagged.get(tag, buf);
    }

protected:
    [[nodiscard]] virtual bool onPacket(const Packet& pkt) noexcept;
    virtual void onFault(Fault reason) noexcept;

    virtual void onGet(const msg::Get& body, uint8_t txn_id) noexcept;
    [[nodiscard]] virtual bool onSetBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                                           IBuf& buf) noexcept;
    virtual void onSet(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept;
    virtual void onReply(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept;

    ReplyWnd _reply;
    LinkHb _link;
    MultiByte _mb;
    TaggedOps _tagged;

private:
    friend class Node;
    friend class TaggedOps;
    template <typename, typename>
    friend class MISC::ObjRegistry;

    void set_id(uint8_t id) noexcept { _id = id; }

    void tick() noexcept;
    [[nodiscard]] const TxSlot* peekTx() noexcept;
    void onTxResult(bool ok) noexcept;

    enum class OutSrc : uint8_t { None, Ctrl, Req };

    [[nodiscard]] uint8_t allocTxn() noexcept;
    [[nodiscard]] bool enqueueReq(Message msg, uint8_t txn_id) noexcept;
    void clearTx() noexcept;
    [[nodiscard]] const TxSlot* peekCtrl() noexcept;

    uint8_t _id = 0;
    uint8_t _peer_id = 0;
    uint8_t _txn_tx = 0;

    bool _master = false;
    OutSrc _out = OutSrc::None;
    bool _rr_ctrl = true;

    TxQueue<SMCP_SESSION_CTRL_TX_CAPACITY> _tx_ctrl;
};

template <uint8_t N, typename S = Session>
using SessionBank = MISC::ObjBank<S, N, false>;

} // namespace smcp
