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
    [[nodiscard]] MultiByte::Error lastSetErr() const noexcept { return _last_set_err; }
    [[nodiscard]] MultiByte::Error lastReplyErr() const noexcept { return _last_reply_err; }
    [[nodiscard]] uint16_t lastReplyLen() const noexcept { return _last_reply_len; }
    [[nodiscard]] const uint8_t* store() const noexcept { return _store; }
    [[nodiscard]] uint16_t storeLen() const noexcept { return _store_len; }
    [[nodiscard]] const uint8_t* lastReply() const noexcept { return _last_reply; }

    void clearFlags() noexcept
    {
        _last_set_ok = false;
        _last_reply_ok = false;
        _last_set_err = MultiByte::Error::None;
        _last_reply_err = MultiByte::Error::None;
        _last_reply_len = 0;
    }

protected:
    void onGet(const msg::Get& body, uint8_t txn_id) noexcept override;
    [[nodiscard]] bool onSetBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                                   IBuf& buf) noexcept override;
    void onSet(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept override;
    void onReply(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept override;
    void onFault(Fault reason) noexcept override;

private:
    SimNode& _owner;
    uint8_t _store[kStoreCap]{};
    uint16_t _store_len = 0;
    uint8_t _rx_scratch[kStoreCap]{};
    uint8_t _last_reply[kStoreCap]{};
    bool _last_set_ok = false;
    bool _last_reply_ok = false;
    MultiByte::Error _last_set_err = MultiByte::Error::None;
    MultiByte::Error _last_reply_err = MultiByte::Error::None;
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
    [[nodiscard]] SessionReg& sessions() noexcept override { return _sessions; }

    void onStatus(Status status) noexcept override
    {
        std::printf("[0x%02X] Node status → %s\n",
            static_cast<unsigned>(id()), cstr(status));
        if (status == Status::LinkError || status == Status::RegisterFailed
            || status == Status::IdConflict) {
            logError("NodeStatus", cstr(status));
        }
    }

    void onLink(Session* session, bool up) noexcept override
    {
        const uint8_t peer = session != nullptr ? session->peerId() : 0u;
        std::printf("[0x%02X] link %s peer=0x%02X\n",
            static_cast<unsigned>(id()), up ? "UP" : "DOWN",
            static_cast<unsigned>(peer));
    }

    void onFault(Session* session, Fault reason) noexcept override
    {
        char detail[80]{};
        if (session != nullptr) {
            std::snprintf(detail, sizeof(detail), "session peer=0x%02X %s",
                static_cast<unsigned>(session->peerId()), cstr(reason));
        } else {
            std::snprintf(detail, sizeof(detail), "node %s", cstr(reason));
        }
        std::printf("[0x%02X] fault %s\n", static_cast<unsigned>(id()), detail);
        logError("NodeFault", detail);
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

    void onAck(Session* /*session*/, const TxSlot& req, const msg::Ack& /*reply*/) noexcept override
    {
        _last_ack = true;
        _last_nack = false;
        std::printf("[0x%02X] Ack req=0x%02X txn=%u\n",
            static_cast<unsigned>(id()),
            static_cast<unsigned>(req.msg.id),
            static_cast<unsigned>(req.txn_id));
    }

    void onNack(Session* /*session*/, const TxSlot& req, const msg::Nack& reply) noexcept override
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
    SessionStore<kMaxSessions> _sessions;
    SessionBank<kMaxSessions, SimSession> _bank;
};

inline SimSession::SimSession(SimNode& node) noexcept
    : Session(node)
    , _owner(node)
{}

inline void SimSession::onGet(const msg::Get& body, uint8_t txn_id) noexcept
{
    std::printf("[0x%02X] onGet tag=0x%02X txn=%u store_len=%u policy=%s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(body.tag),
        static_cast<unsigned>(txn_id),
        static_cast<unsigned>(_store_len),
        SimNode::policyName(_owner.rxPolicy()));
    switch (_owner.rxPolicy()) {
    case RxPolicy::Reject:
        sendNack(txn_id, msg::Nack::Error::NotSupported);
        return;
    case RxPolicy::Busy:
        sendNack(txn_id, msg::Nack::Error::Busy);
        return;
    case RxPolicy::Echo:
        break;
    }
    if (body.tag != kTagEcho || _store_len == 0u) {
        sendNack(txn_id, msg::Nack::Error::NotSupported);
        return;
    }
    const IBuf buf{_store, _store_len};
    if (!send(body.tag, buf, txn_id)) {
        sendNack(txn_id, msg::Nack::Error::Busy);
    }
}

inline bool SimSession::onSetBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                                    IBuf& buf) noexcept
{
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

inline void SimSession::onSet(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept
{
    _last_set_err = status;
    _last_set_ok = (status == MultiByte::Error::None && tag == kTagEcho && buf.data != nullptr);
    if (_last_set_ok) {
        _store_len = buf.size;
        std::memcpy(_store, buf.data, _store_len);
    }
    std::printf("[0x%02X] onSet tag=0x%02X len=%u status=%s → %s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(tag),
        static_cast<unsigned>(buf.size),
        MultiByte::cstr(status),
        _last_set_ok ? "stored" : "reject");
}

inline void SimSession::onReply(uint8_t tag, const IBuf& buf, MultiByte::Error status) noexcept
{
    _last_reply_err = status;
    _last_reply_ok = (status == MultiByte::Error::None && tag == kTagEcho);
    _last_reply_len = 0;
    if (buf.data != nullptr && buf.size > 0u) {
        _last_reply_len = buf.size <= kStoreCap ? buf.size : kStoreCap;
        std::memcpy(_last_reply, buf.data, _last_reply_len);
    }
    std::printf("[0x%02X] onReply tag=0x%02X len=%u status=%s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(tag),
        static_cast<unsigned>(buf.size),
        MultiByte::cstr(status));
}

inline void SimSession::onFault(Fault reason) noexcept
{
    std::printf("[0x%02X] session fault peer=0x%02X %s\n",
        static_cast<unsigned>(_owner.id()),
        static_cast<unsigned>(peerId()),
        cstr(reason));
    char detail[64]{};
    std::snprintf(detail, sizeof(detail), "peer=0x%02X %s",
        static_cast<unsigned>(peerId()), cstr(reason));
    _owner.logError("SessionFault", detail);
}

} // namespace host
} // namespace smcp
