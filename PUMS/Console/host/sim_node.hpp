/**
 * @file sim_node.hpp
 * @brief Host Node + Session: KV echo, multi-session, error log.
 */

#pragma once

#include <cstdio>
#include <cstring>

#include "error_log.hpp"
#include "nack_fmt.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

namespace smcp {
namespace host {

inline constexpr uint8_t kTagEcho = 0xA1u;
inline constexpr uint16_t kStoreCap = 256u;
#ifndef SMCP_HOST_MAX_SESSIONS
#define SMCP_HOST_MAX_SESSIONS 16u
#endif

class SimNode;

/** Как этот узел отвечает на чужие Get/Set (транспортные Nack). */
enum class RxPolicy : uint8_t {
    Echo = 0, /**< tag 0xA1: store / reply */
    Reject,   /**< Nack NotSupported */
    Busy,     /**< Get → Nack Busy; Set → NotSupported */
};

class SimSession : public Session {
public:
    explicit SimSession(SimNode& node) noexcept;

    [[nodiscard]] bool lastSetOk() const noexcept { return _last_set_ok; }
    [[nodiscard]] bool lastReplyOk() const noexcept { return _last_reply_ok; }
    [[nodiscard]] session::MultiByte::Error lastSetErr() const noexcept { return _last_set_err; }
    [[nodiscard]] session::MultiByte::Error lastReplyErr() const noexcept { return _last_reply_err; }
    [[nodiscard]] uint16_t lastReplyLen() const noexcept { return _last_reply_len; }
    [[nodiscard]] const uint8_t* store() const noexcept { return _store; }
    [[nodiscard]] uint16_t storeLen() const noexcept { return _store_len; }
    [[nodiscard]] const uint8_t* lastReply() const noexcept { return _last_reply; }

    void clearFlags() noexcept
    {
        _last_set_ok = false;
        _last_reply_ok = false;
        _last_set_err = session::MultiByte::Error::None;
        _last_reply_err = session::MultiByte::Error::None;
        _last_reply_len = 0;
    }

protected:
    void onGet(const session::Get& body, uint8_t txn_id) noexcept override;
    [[nodiscard]] bool onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                                   uint16_t size, IBuf& buf) noexcept override;
    [[nodiscard]] session::vError onSet(uint8_t tag, uint8_t index,
                                   const IBuf& buf) noexcept override;
    void onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                 session::MultiByte::Error status) noexcept override;

private:
    SimNode& _owner;
    uint8_t _store[kStoreCap]{};
    uint16_t _store_len = 0;
    uint8_t _rx_scratch[kStoreCap]{};
    uint8_t _last_reply[kStoreCap]{};
    bool _last_set_ok = false;
    bool _last_reply_ok = false;
    session::MultiByte::Error _last_set_err = session::MultiByte::Error::None;
    session::MultiByte::Error _last_reply_err = session::MultiByte::Error::None;
    uint16_t _last_reply_len = 0;
};

class SimNode : public Node {
public:
    static constexpr uint8_t kMaxSessions = SMCP_HOST_MAX_SESSIONS;

    SimNode(ILink& link, ClockFn clock, ErrorLog* errors = nullptr) noexcept
        : Node(link, clock)
        , _errors(errors)
        , _bank(*this)
    {}

    void setErrorLog(ErrorLog* errors) noexcept { _errors = errors; }

    void setRxPolicy(RxPolicy policy) noexcept { _rx_policy = policy; }
    [[nodiscard]] RxPolicy rxPolicy() const noexcept { return _rx_policy; }

    [[nodiscard]] static const char* policyName(RxPolicy policy) noexcept
    {
        switch (policy) {
        case RxPolicy::Echo: return "echo";
        case RxPolicy::Reject: return "reject";
        case RxPolicy::Busy: return "busy";
        }
        return "?";
    }

    void logError(const char* kind, const char* detail) noexcept
    {
        if (_errors != nullptr) {
            _errors->push(id(), kind, detail);
        } else {
            std::fprintf(stderr, "[ERR] node=0x%02X %s: %s\n",
                static_cast<unsigned>(id()),
                kind != nullptr ? kind : "?",
                detail != nullptr ? detail : "");
        }
    }

    /** Master-сессия к peer (Idle slot или уже существующая). */
    [[nodiscard]] SimSession* startPeer(uint8_t peer_id) noexcept
    {
        if (!isReady() || peer_id == 0u) {
            return nullptr;
        }
        if (Session* existing = sessionByPeer(peer_id)) {
            if (existing->getStatus() == Session::Status::Idle) {
                existing->start(peer_id);
            }
            return static_cast<SimSession*>(existing);
        }
        Session* const slot = idleSession();
        if (slot == nullptr) {
            onSessionFull(peer_id);
            return nullptr;
        }
        slot->start(peer_id);
        return static_cast<SimSession*>(slot);
    }

    [[nodiscard]] SimSession* peer(uint8_t peer_id) noexcept
    {
        return static_cast<SimSession*>(sessionByPeer(peer_id));
    }

    [[nodiscard]] SimSession& sess() noexcept { return _bank[0]; }

    void clearAckFlags() noexcept
    {
        _last_ack = false;
        _last_nack = false;
    }
    [[nodiscard]] bool lastAck() const noexcept { return _last_ack; }
    [[nodiscard]] bool lastNack() const noexcept { return _last_nack; }

protected:
    [[nodiscard]] SessionSlots& sessions() noexcept override { return _bank; }

    void onState(State state) noexcept override
    {
        std::printf("[0x%02X] Node state → %s\n",
            static_cast<unsigned>(id()), cstr(state));
    }

    void onError(Error error) noexcept override
    {
        std::printf("[0x%02X] Node error → %s\n",
            static_cast<unsigned>(id()), cstr(error));
        if (error != Error::None) {
            logError("NodeError", cstr(error));
        }
    }

    void onLink(Session* session, Link link) noexcept override
    {
        const uint8_t peer = session != nullptr ? session->peerId() : 0u;
        std::printf("[0x%02X] link %s peer=0x%02X\n",
            static_cast<unsigned>(id()), cstr(link),
            static_cast<unsigned>(peer));
        if ((link != Link::HbLost && link != Link::PeerDown) || session == nullptr) {
            return;
        }
        char detail[64]{};
        std::snprintf(detail, sizeof(detail), "peer=0x%02X %s",
            static_cast<unsigned>(peer), cstr(link));
        logError("SessionFault", detail);
    }

    void onSessionFull(uint8_t peer_id) noexcept override
    {
        char detail[40]{};
        std::snprintf(detail, sizeof(detail), "peer=0x%02X", static_cast<unsigned>(peer_id));
        logError("SessionFull", detail);
    }

    void onTxFull(Session* session) noexcept override
    {
        char detail[40]{};
        std::snprintf(detail, sizeof(detail), "session=%s",
            session != nullptr ? "yes" : "bus");
        logError("TxFull", detail);
    }

    void onAck(Session* /*session*/, const TxSlot& req, const session::Ack& /*reply*/) noexcept override
    {
        _last_ack = true;
        _last_nack = false;
        std::printf("[0x%02X] Ack req=0x%02X txn=%u\n",
            static_cast<unsigned>(id()),
            static_cast<unsigned>(req.msg.id),
            static_cast<unsigned>(req.txn_id));
    }

    void onNack(Session* /*session*/, const TxSlot& req, const session::Nack& reply) noexcept override
    {
        _last_ack = false;
        _last_nack = true;
        char detail[160]{};
        formatNack(detail, sizeof(detail), req.msg.id, req.txn_id, reply);
        std::printf("[0x%02X] Nack %s\n", static_cast<unsigned>(id()), detail);
        logError("Nack", detail);
    }

    void onPackFailed(Session* /*session*/, uint8_t msg_id) noexcept override
    {
        char detail[32]{};
        std::snprintf(detail, sizeof(detail), "msg_id=0x%02X", static_cast<unsigned>(msg_id));
        logError("PackFailed", detail);
    }

private:
    ErrorLog* _errors = nullptr;
    RxPolicy _rx_policy = RxPolicy::Echo;
    bool _last_ack = false;
    bool _last_nack = false;
    SessionBank<kMaxSessions, SimSession> _bank;
};

inline SimSession::SimSession(SimNode& node) noexcept
    : Session(node)
    , _owner(node)
{}

inline void SimSession::onGet(const session::Get& body, uint8_t txn_id) noexcept
{
    std::printf("[0x%02X] onGet tag=0x%02X index=0x%02X txn=%u store_len=%u policy=%s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(body.tag),
        static_cast<unsigned>(body.index),
        static_cast<unsigned>(txn_id),
        static_cast<unsigned>(_store_len),
        SimNode::policyName(_owner.rxPolicy()));
    switch (_owner.rxPolicy()) {
    case RxPolicy::Reject:
        sendNack(txn_id, session::Error::NotSupported);
        return;
    case RxPolicy::Busy:
        sendNack(txn_id, session::Error::Busy);
        return;
    case RxPolicy::Echo:
        break;
    }
    if (body.tag != kTagEcho || _store_len == 0u) {
        sendNack(txn_id, session::Error::NotSupported);
        return;
    }
    const IBuf buf{_store, _store_len};
    if (!send(body.tag, buf, txn_id, body.index)) {
        sendNack(txn_id, session::Error::Busy);
    }
}

inline bool SimSession::onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                                    uint16_t size, IBuf& buf) noexcept
{
    (void)index;
    (void)txn_id;
    if (_owner.rxPolicy() != RxPolicy::Echo) {
        return false;
    }
    if (tag != kTagEcho || size == 0u || size > kStoreCap) {
        return false;
    }
    buf = {_rx_scratch, size};
    return true;
}

inline session::vError SimSession::onSet(uint8_t tag, uint8_t index, const IBuf& buf) noexcept
{
    _last_set_err = session::MultiByte::Error::None;
    _last_set_ok = (tag == kTagEcho && buf.data != nullptr);
    if (_last_set_ok) {
        _store_len = buf.size;
        std::memcpy(_store, buf.data, _store_len);
    }
    std::printf("[0x%02X] onSet tag=0x%02X index=0x%02X len=%u → %s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(tag),
        static_cast<unsigned>(index),
        static_cast<unsigned>(buf.size),
        _last_set_ok ? "stored" : "reject");
    return _last_set_ok ? session::Error::None : session::Error::NotSupported;
}

inline void SimSession::onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                                session::MultiByte::Error status) noexcept
{
    _last_reply_err = status;
    _last_reply_ok = (status == session::MultiByte::Error::None && tag == kTagEcho);
    _last_reply_len = 0;
    if (buf.data != nullptr && buf.size > 0u) {
        _last_reply_len = buf.size <= kStoreCap ? buf.size : kStoreCap;
        std::memcpy(_last_reply, buf.data, _last_reply_len);
    }
    std::printf("[0x%02X] onReply tag=0x%02X index=0x%02X len=%u status=%s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(tag),
        static_cast<unsigned>(index),
        static_cast<unsigned>(buf.size),
        session::MultiByte::cstr(status));
}

} // namespace host
} // namespace smcp
