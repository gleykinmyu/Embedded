/**
 * @file comm/session.hpp
 * @brief comm::Session / SessionBank — протоколы одного peer и пул сессий.
 *
 * Session — контейнер протоколов peer; peekTx/onPacket только пока open.
 * onPacket: только кадры с src == peer. Broadcast — протоколы на Node.
 *
 * SessionSlots — банк без N: родитель — Node (ProtoChild), find/open через forEach.
 * SessionBank&lt;N&gt; = ProtoBank&lt;Sess, N, SessionSlots&lt;…&gt;&gt;: массив слотов.
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "comm/node.hpp"
#include "comm/old/container.hpp"

namespace comm {

template <typename Packet>
class Session : public ProtoContainer<Packet> {
public:
    using NodeT = Node<Packet>;
    using Proto = IProto<Packet>;
    using addr_t = typename NodeT::addr_t;

    /** Слот банка: attach в банк. */
    explicit Session(ProtoContainer<Packet>& box) noexcept { box.attach(*this); }

    [[nodiscard]] addr_t peer() const noexcept { return _peer; }
    [[nodiscard]] bool isOpen() const noexcept { return _open; }

    void open(addr_t peer) noexcept
    {
        if (_open) {
            close();
        }
        _peer = peer;
        _open = true;
    }

    void close() noexcept
    {
        if (!_open) {
            return;
        }
        this->abort();
        _open = false;
        _peer = addr_t::kNone;
    }

    [[nodiscard]] bool peekTx(Proto*& owner, Packet& out,
                              MISC::time_t now) noexcept override
    {
        if (!_open) {
            return false;
        }
        return ProtoContainer<Packet>::peekTx(owner, out, now);
    }

    [[nodiscard]] bool onPacket(const Packet& frame, MISC::time_t now) noexcept override
    {
        if (!_open || frame.src != _peer) {
            return false;
        }
        return ProtoContainer<Packet>::onPacket(frame, now);
    }

    void abort() noexcept override
    {
        ProtoContainer<Packet>::abort();
        _open = false;
        _peer = addr_t::kNone;
    }

private:
    addr_t _peer = addr_t::kNone;
    bool _open = false;
};

/**
 * Пул сессий без размера: слоты уже в списке предка ProtoSlots.
 * find — открытая с peer; open — find или свободный слот + Sess::open.
 */
template <typename Packet, typename Sess = Session<Packet>>
class SessionSlots : public ProtoSlots<Sess, Packet> {
    static_assert(std::is_base_of_v<Session<Packet>, Sess>,
                  "SessionSlots: Sess must derive from comm::Session<Packet>");

public:
    using NodeT = Node<Packet>;
    using SessionT = Sess;
    using addr_t = typename SessionT::addr_t;

    /** Открытая сессия с @a peer; нет — nullptr. */
    [[nodiscard]] SessionT* find(addr_t peer) noexcept
    {
        if (peer == addr_t::kNone) {
            return nullptr;
        }
        SessionT* found = nullptr;
        this->forEach([&](SessionT& s) -> bool {
            if (s.isOpen() && s.peer() == peer) {
                found = &s;
                return true;
            }
            return false;
        });
        return found;
    }

    [[nodiscard]] const SessionT* find(addr_t peer) const noexcept
    {
        return const_cast<SessionSlots*>(this)->find(peer);
    }

    /**
     * Сессия к @a peer: уже открыта → она; иначе свободный слот и Sess::open.
     * @return nullptr — peer none / нет свободного слота.
     */
    [[nodiscard]] SessionT* open(addr_t peer) noexcept
    {
        if (peer == addr_t::kNone) {
            return nullptr;
        }
        if (SessionT* s = find(peer)) {
            return s;
        }
        SessionT* slot = nullptr;
        this->forEach([&](SessionT& s) -> bool {
            if (!s.isOpen()) {
                slot = &s;
                return true;
            }
            return false;
        });
        if (slot == nullptr) {
            return nullptr;
        }
        slot->open(peer);
        return slot;
    }

    /** RX: сразу в сессию по src. */
    [[nodiscard]] bool onPacket(const Packet& frame, MISC::time_t now) noexcept override
    {
        SessionT* s = find(frame.src);
        if (s == nullptr) {
            return false;
        }
        return s->onPacket(frame, now);
    }

protected:
    explicit SessionSlots(NodeT& node) noexcept
        : ProtoSlots<Sess, Packet>(node)
    {}
};

/** Session[N] на Node. Массив и at() — у ProtoBank, база — SessionSlots. */
template <typename Packet, uint8_t N, typename Sess = Session<Packet>>
using SessionBank = ProtoBank<Sess, N, SessionSlots<Packet, Sess>>;

} // namespace comm
