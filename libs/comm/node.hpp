/**
 * @file comm/node.hpp
 * @brief comm::Node — корень протоколов + окно TX-слотов + ILink.
 *
 * Дети (Session / broadcast IProto) — через ProtoContainer.
 * Кадры не копятся: свободный слот → peekTx → link.send; pollTx → owner->onTxDone.
 * RX: в Listen кадр видит только _ncp; в Ready — onPacket по списку.
 *
 * Окно TX — у ILink. Node кладёт owner в cookie send и забирает его из pollTx.
 *
 * Frame (больше полей Node не трогает):
 *   - конструируется как Frame{};
 *   - src и dst одного типа addr_t, оба присваиваются.
 * addr_t:
 *   - static const addr_t kNone — пустой адрес;
 *   - копируется и сравнивается (==, !=);
 *   - bool accepts(addr_t dst) const — кадр наш (unicast / broadcast / подсеть).
 *     Node про широковещание не знает.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "comm/container.hpp"
#include "comm/ilink.hpp"
#include "comm/iproto.hpp"
#include "ms_timer.hpp"

namespace comm {

template <typename Frame>
class NcpBase;

/**
 * Узел на среде: ILink + контейнер протоколов. Окно TX — у линка.
 */
template <typename Frame>
class Node : public ProtoContainer<Frame> {
    friend class NcpBase<Frame>;

public:
    using Proto = IProto<Frame>;
    using Link = ILink<Frame>;
    using addr_t = std::decay_t<decltype(std::declval<Frame>().src)>;
    using TxResult = typename Link::TxResult;

    static_assert(std::is_same_v<addr_t, std::decay_t<decltype(std::declval<Frame>().dst)>>,
                  "Node: Frame::src and Frame::dst must have the same type");

    enum class State : uint8_t {
        Idle = 0, ///< до begin / после end
        Listen,   ///< id на среде; кадры видит только _ncp. nullptr → goReady()
        Ready,    ///< TX/RX протоколов
    };

    enum class Error : uint8_t {
        None = 0,
        AddrConflict, ///< на среде кадр с src == наш id
    };

    explicit Node(Link& link) noexcept
        : _link(link)
    {}

    [[nodiscard]] Link& link() noexcept { return _link; }
    [[nodiscard]] const Link& link() const noexcept { return _link; }

    [[nodiscard]] addr_t selfAddr() const noexcept { return _self_addr; }
    [[nodiscard]] State state() const noexcept { return _state; }
    [[nodiscard]] Error error() const noexcept { return _error; }
    [[nodiscard]] std::size_t txBusy() const noexcept { return _link.txBusy(); }

    /** Idle → Listen (id на среде). kNone — пустой адрес, узел не трогаем. */
    void begin(addr_t self) noexcept
    {
        if (self == addr_t::kNone) {
            return;
        }
        end();
        _self_addr = self;
        _error = Error::None;
        setState(State::Listen);
    }

    /** Сброс слотов, abort детей, Idle. */
    void end() noexcept
    {
        failAllTx();
        this->abort();
        _self_addr = addr_t::kNone;
        _error = Error::None;
        setState(State::Idle);
    }

    void clearError() noexcept
    {
        if (_error == Error::None) {
            return;
        }
        _error = Error::None;
        if (_state == State::Idle && _self_addr != addr_t::kNone) {
            setState(State::Listen);
        }
    }

    /** pollTx → onTxDone; receive → demux; свободные слоты ← peekTx; tick детям. */
    void update(MISC::time_t now) noexcept
    {
        if (_state == State::Idle) {
            return;
        }

        completeTx();
        pumpRx(now);

        if (_state == State::Idle) {
            return;
        }

        if (_state == State::Listen) {
            if (_ncp != nullptr) {
                _ncp->tick(now);
            } else {
                goReady();
            }
        }

        if (_state == State::Ready && _link.isOpen()) {
            fillTx(now);
        }

        if (_state == State::Ready) {
            this->tick(now);
        }
    }

protected:
    /**
     * Net check protocol. В списке — как обычный ребёнок; в Listen Node
     * зовёт именно _ncp, не голову списка.
     * @return false — NCP уже есть или @a proto уже в каком-то списке.
     */
    bool attachNCP(Proto& proto) noexcept
    {
        if (_ncp != nullptr || !this->attachFront(proto)) {
            return false;
        }
        _ncp = &proto;
        return true;
    }

    /** Переход в Error::AddrConflict, узел в Idle. */
    void goAddrConflict() noexcept { setError(Error::AddrConflict); }

    /** Переход Listen → Ready. */
    void goReady() noexcept
    {
        if (_state == State::Listen) {
            setState(State::Ready);
        }
    }

    virtual void onState(State) noexcept {}
    virtual void onError(Error) noexcept {}

    /** Кадр нам — решает addr_t: self.accepts(dst). */
    [[nodiscard]] bool accepts(const Frame& frame) const noexcept
    {
        return _self_addr.accepts(frame.dst);
    }

private:
    void setState(State st) noexcept
    {
        if (_state == st) {
            return;
        }
        _state = st;
        onState(st);
    }

    void setError(Error err) noexcept
    {
        _error = err;
        onError(err);
        if (err == Error::AddrConflict) {
            failAllTx();
            this->abort();
            setState(State::Idle);
        }
    }

    void fillTx(MISC::time_t now) noexcept
    {
        while (_link.txSpace() > 0u) {
            Proto* owner = nullptr;
            Frame frame{};
            if (!this->peekTx(owner, frame, now) || owner == nullptr) {
                break;
            }
            frame.src = _self_addr;
            if (!_link.send(frame, owner)) {
                owner->onTxDone(frame, false);
                break;
            }
        }
    }

    void completeTx() noexcept
    {
        Frame frame{};
        void* cookie = nullptr;
        TxResult result = TxResult::Ok;
        while (_link.pollTx(frame, cookie, result)) {
            if (cookie != nullptr) {
                static_cast<Proto*>(cookie)->onTxDone(frame, result == TxResult::Ok);
            }
        }
    }

    void failAllTx() noexcept
    {
        _link.abortTx();
        completeTx();
    }

    void pumpRx(MISC::time_t now) noexcept
    {
        Frame frame{};
        while (_link.receive(frame)) {
            if (frame.src == addr_t::kNone) {
                continue;
            }
            if (_state == State::Listen) {
                if (_ncp != nullptr) {
                    (void)_ncp->onPacket(frame, now);
                }
                if (_state != State::Listen) {
                    return;
                }
                continue;
            }
            if (_state != State::Ready || !accepts(frame)) {
                continue;
            }
            (void)this->onPacket(frame, now);
            if (_state != State::Ready) {
                return;
            }
        }
    }

    Link& _link;
    Proto* _ncp = nullptr;
    addr_t _self_addr = addr_t::kNone;
    State _state = State::Idle;
    Error _error = Error::None;
};

/**
 * База протокола проверки адреса. Сама зовёт attachNCP.
 * В Listen узел обращается к ней по _ncp, не по месту в списке.
 * Потомкам — ready() и addrConflict().
 */
template <typename Frame>
class NcpBase : public ProtoBase<Frame> {
public:
    using NodeT = Node<Frame>;

protected:
    explicit NcpBase(NodeT& node) noexcept
        : _node(node)
    {
        _node.attachNCP(*this);
    }

    void ready() noexcept { _node.goReady(); }
    void addrConflict() noexcept { _node.goAddrConflict(); }

    NodeT& _node;
};

/**
 * Проверка адреса на проводе.
 * В Listen Node зовёт onPacket по _ncp: src == наш адрес → addrConflict().
 * Окно истекло без конфликта → ready().
 */
template <typename Frame>
class Ncp : public NcpBase<Frame> {
public:
    using NodeT = typename NcpBase<Frame>::NodeT;

    /** Окно тишины на шине, как у прежнего Node (~Heartbeat). */
    static constexpr MISC::time_t kListenMs = 200u;

    explicit Ncp(NodeT& node, MISC::time_t listenMs = kListenMs) noexcept
        : NcpBase<Frame>(node)
        , _listenMs(listenMs)
    {}

    [[nodiscard]] bool onPacket(const Frame& frame, MISC::time_t) noexcept override
    {
        const auto self = this->_node.selfAddr();
        if (self == decltype(self)::kNone || frame.src != self) {
            return false;
        }
        this->addrConflict();
        return true;
    }

    void tick(MISC::time_t now) noexcept override
    {
        if (this->_node.state() != NodeT::State::Listen) {
            _listen.stop();
            return;
        }
        _listen.startOnce(now, _listenMs);
        if (_listen.timedOut(now)) {
            _listen.stop();
            this->ready();
        }
    }

    void abort() noexcept override { _listen.stop(); }

protected:
    [[nodiscard]] bool peekTx(Frame&, MISC::time_t) noexcept override { return false; }

private:
    MISC::time_t _listenMs;
    MISC::MsTimer _listen{};
};

} // namespace comm
