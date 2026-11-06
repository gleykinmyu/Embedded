/**
 * @file server.hpp
 * @brief segment::{IServer, Server} + server::{Axis, Session}.
 *
 * IServer / Server — фасады сегмента (в segment). Axis / Session — в server.
 * SessionBank — у leaf (MServer); inventory — server::AxisStore здесь или у leaf.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "obj_bank.hpp"
#include "obj_registry.hpp"
#include "segment/axis.hpp"
#include "segment/message.hpp"
#include "smcp/ilink.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

namespace smcp {
namespace segment {

namespace server {
class Axis;
class Session;
/** Phys axis_id / ёмкость inventory (Id у ObjRegistry / ObjStorage). */
using idx_t = uint8_t;
/** Банк осей[N]: @a AxisT : Axis; ctor регистрирует в IServer. */
template <typename AxisT, idx_t N>
using AxisBank = MISC::ObjBank<AxisT, N>;
using AxisReg = MISC::ObjRegistry<Axis, idx_t>;
template <idx_t Cap>
using AxisStore = MISC::ObjStorage<Axis, Cap, idx_t, 0>;
} // namespace server

class IServer;

namespace server {

/**
 * IAxis сегмента: commit Select/Block; паспорт; live; register в IServer.
 * setTarget / resetFault — pure virtual, реализует leaf (привод).
 * Лимит Selected — leaf `acceptSelect` / `kMaxSelected`.
 */
class Axis : public IAxis {
public:
    static constexpr uint8_t kMaxSelected = 3u;

    axis::Profile profile{};

    Axis(IServer& owner, idx_t axis_id) noexcept;

    /** Phys axis_id в сегменте (0…31); слот ObjRegistry. */
    [[nodiscard]] idx_t axisId() const noexcept { return _axis_id; }
    [[nodiscard]] int32_t position() const noexcept override { return _position; }
    [[nodiscard]] REG::BitMask<axis::Status> status() const noexcept override { return _status; }
    [[nodiscard]] uint8_t holder() const noexcept override { return _holder; }

    [[nodiscard]] bool select(uint8_t console_id) noexcept override;
    [[nodiscard]] bool block(bool blocked) noexcept override;

    void setPosition(int32_t position) noexcept { _position = position; }

    void setStatus(REG::BitMask<axis::Status> status) noexcept { _status = status; }
    void setStatus(axis::Status bit, bool on) noexcept
    {
        if (on) {
            _status.set(bit);
        } else {
            _status.clear(bit);
        }
    }

protected:
    /** ObjRegistry::registerAt — слот = phys axis_id. */
    void set_id(idx_t axis_id) noexcept { _axis_id = axis_id; }

    idx_t _axis_id = 0;
    uint8_t _holder = kHolderNone;
    int32_t _position = 0;
    REG::BitMask<axis::Status> _status{};

    template <typename, typename>
    friend class MISC::ObjRegistry;
};

namespace detail {
/** Регистрация оси в inventory сервера (вызывается из Axis ctor). */
void registerAxis(IServer& server, Axis& axis) noexcept;
} // namespace detail

} // namespace server

/** Узел сегмента: inventory server::Axis* + политика Select/Block/SetTarget + Telemetry. */
class IServer : public Node {
public:
    virtual ~IServer() = default;

    // --- оси (lookup) ---

    [[nodiscard]] server::idx_t axisCapacity() const noexcept
    {
        return static_cast<server::idx_t>(const_cast<IServer*>(this)->storage().capacity());
    }

    [[nodiscard]] server::Axis* axis(server::idx_t id) noexcept { return storage().get(id); }
    [[nodiscard]] const server::Axis* axis(server::idx_t id) const noexcept
    {
        return const_cast<IServer*>(this)->storage().get(id);
    }
    /** Inventory + revision паспортов (registerAxis / Set AxisProfile). */
    [[nodiscard]] const AxisRegisteredInfo& axisRegistered() const noexcept
    {
        return _axisRegistered;
    }
    /** ++revision после успешного Set AxisProfile. */
    void bumpAxisRegisteredRevision() noexcept { ++_axisRegistered.revision; }

    /**
     * Node::begin, только `isServerId`. Listen → Ready в Node::update.
     */
    void begin(uint8_t server_id) noexcept;

    // --- исходящие PDU ---

    /** Broadcast Telemetry (класс D) по текущему состоянию оси. */
    void sendTelemetry(server::idx_t axis_id) noexcept;

    /**
     * Эксклюзивный сервисный доступ (одна консоль): атомарность Set AxisProfile.
     * holder = console peer id; 0 — locked.
     */
    [[nodiscard]] bool isServiceUnlocked() const noexcept
    {
        return _serviceHolder != 0u;
    }
    [[nodiscard]] uint8_t serviceHolder() const noexcept { return _serviceHolder; }
    [[nodiscard]] bool isServiceHolder(uint8_t console_id) const noexcept
    {
        return console_id != 0u && _serviceHolder == console_id;
    }

    /**
     * Снять сервис. Только holder (или уже locked — no-op).
     * @return false — держит другая консоль.
     */
    bool lockService(uint8_t console_id) noexcept;

    /**
     * Взять сервис. Свободен или уже эта консоль — ok (обновляет hold timer).
     * @return false — занято другой консолью.
     */
    bool unlockService(uint8_t console_id) noexcept;

    /** Авто-lock по ServiceAccess::kHoldMs. */
    void serviceTick() noexcept;

    /** Хэш PIN сервиса (NVM / leaf). 0 — unlock всегда отказ. */
    [[nodiscard]] uint32_t servicePinHash() const noexcept { return _servicePinHash; }

protected:
    /**
     * Down/Lost: dropPeerSelect этого peer.
     * Error::IdConflict: снять хвосты Select без Telemetry (сессии уже Down).
     */
    void onLink(::smcp::Session* session, Link link) noexcept override;
    void onError(Error error) noexcept override;

    friend void server::detail::registerAxis(IServer& server, server::Axis& axis) noexcept;
    friend class server::Session;

    // --- ctor / storage ---

    explicit IServer(ILink& link, ClockFn clock) noexcept;

    [[nodiscard]] virtual server::AxisReg& storage() noexcept = 0;

    // --- политика (leaf) ---

    /**
     * Можно ли консоли @a console_id держать маску @a selected после запроса.
     * @a selected — итоговое владение этой консоли (после take/drop, без commit).
     * Leaf при общем лимите сливает с чужими holders (напр. SelectLimit).
     */
    [[nodiscard]] virtual vError acceptSelect(uint8_t console_id,
                                              Selection selected) const noexcept
    {
        (void)console_id;
        (void)selected;
        return session::Error::None;
    }

    /**
     * Можно ли консоли @a console_id выставить сегментную маску @a blocked.
     * @a blocked — итоговое Status::Blocked сегмента после запроса (без commit).
     */
    [[nodiscard]] virtual vError acceptBlock(uint8_t console_id,
                                             Selection blocked) const noexcept
    {
        (void)console_id;
        (void)blocked;
        return session::Error::None;
    }

    /**
     * Можно ли консоли @a console_id задать @a target оси @a axis_id.
     * Leaf: лимиты хода / зоны → segment::Error::Limits.
     */
    [[nodiscard]] virtual vError acceptSetTarget(uint8_t console_id,
                                                 uint8_t axis_id,
                                                 const axis::MotionTarget& target) const noexcept
    {
        (void)console_id;
        (void)axis_id;
        (void)target;
        return session::Error::None;
    }

    /**
     * Пустая маска ResetFault: сброс globalFault.
     * Это GPIO сегмента, не оси. Пин — в leaf; база ничего не трогает.
     */
    virtual void resetGlobalFault() noexcept {}

    /** Задать PIN-хэш (leaf / NVM). */
    void setServicePinHash(uint32_t hash) noexcept { _servicePinHash = hash; }

    /** Scratch Set: приём на wire, commit в axis.profile только при Ok. */
    axis::Profile _profileScratch{};

private:
    void dropPeerSelect(server::Session* session, bool telemetry) noexcept;
    AxisRegisteredInfo _axisRegistered{};
    uint32_t _servicePinHash = 0u;
    uint8_t _serviceHolder = 0u;
    uint32_t _serviceUnlockAtMs = 0u;
};

namespace server {

/**
 * Сессия консоли на сервере: class A → Ack/Nack + Telemetry.
 * Get AxisRegistered / Get-Set AxisProfile; ServiceAccess → Set (один axis_id).
 */
class Session : public ::smcp::Session {
public:
    explicit Session(IServer& server) noexcept;

    /** Эта сессия держит эксклюзивный ServiceAccess. */
    [[nodiscard]] bool isServiceUnlocked() const noexcept
    {
        return _server.isServiceHolder(peerId());
    }

    /** segment::Error и session::Error → транспортный sendNack. */
    inline void sendNack(uint8_t txn_id, vError error,
                  uint32_t detail = session::Nack::kDetailNone) noexcept
    {
        ::smcp::Session::sendNack(txn_id, error, detail);
    }

protected:
    // --- RX ---

    [[nodiscard]] bool onPacket(const Packet& pkt) noexcept override;
    void tick() noexcept override;

    void onGet(const session::Get& body, uint8_t txn_id) noexcept override;
    [[nodiscard]] bool onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn_id,
                                   uint16_t size, IBuf& buf) noexcept override;
    [[nodiscard]] session::vError onSet(uint8_t tag, uint8_t index,
                                   const IBuf& buf) noexcept override;

private:
    /** Select (holder) vs Block (Status::Blocked) — общий пайплайн plan→accept→commit. */
    enum class MaskKind : uint8_t { Select, Block };

    /**
     * План изменения маски для Action Add/Remove/Set (без commit).
     * was = наше владение (Select) или isBlocked (Block).
     */
    struct MaskPlan {
        Selection take{}; /**< Станет true. */
        Selection drop{}; /**< Станет false. */
        Selection next{}; /**< Итог для accept*. */

        void note(uint8_t mid, bool was, bool in_mask, Action action) noexcept;
        [[nodiscard]] Selection changed() const noexcept { return take | drop; }
    };

    /**
     * Общий путь Select/Block: план → accept* → commit → Ack → Telemetry.
     * Пропуск осей с !in_mask && !was (нет изменений).
     */
    void handleMaskOp(MaskKind kind,
                      Action action,
                      Selection selection,
                      uint8_t txn_id) noexcept;

    /**
     * SetTarget: одна ось или all_selected (Select этой консоли).
     * Сначала проверка всех → commit → Ack → Telemetry.
     */
    void onSetTarget(const SetTarget& body, uint8_t txn_id) noexcept;

    /** GetTelemetry: проверка маски → Ack → Telemetry по осям (без commit). */
    void onGetTelemetry(const GetTelemetry& body, uint8_t txn_id) noexcept;

    /**
     * Пустая маска → resetGlobalFault, Ack, без Telemetry.
     * Биты → resetFault по осям, Ack + Telemetry. Чужая ось — Nack AxisNotFound.
     */
    void onResetFault(const ResetFault& body, uint8_t txn_id) noexcept;

    /**
     * Бит маски не в axisRegistered → Nack AxisNotFound, detail = лишние биты.
     * @return true — уже Nack, выйти.
     */
    [[nodiscard]] bool checkMechId(Selection selection, uint8_t txn_id) noexcept;

    /**
     * Проверка доступа к оси.
     * @param must_own true = SetTarget (нужна наша + drive);
     *                 false = Select (чужой → Busy; свободная → drive; своя → Ok).
     */
    [[nodiscard]] static vError mechGuard(const Axis& axis,
                                          uint8_t src,
                                          bool must_own) noexcept;

    void commitSelect(uint8_t src, const MaskPlan& plan) noexcept;
    void commitBlock(const MaskPlan& plan) noexcept;
    void sendTelemetryMask(Selection mask) noexcept;

    void onServiceAccess(const ServiceAccess& body, uint8_t txn_id) noexcept;
    void onGetAxisRegistered(uint8_t txn_id) noexcept;
    void onGetAxisProfile(uint8_t index, uint8_t txn_id) noexcept;
    [[nodiscard]] vError onSetAxisProfile(uint8_t index,
                                          const IBuf& buf) noexcept;

    /** index > kAxisIdMax (и не ObjTag::kIndexAll) → Nack AxisNotFound. */
    [[nodiscard]] bool checkAxisIndex(uint8_t index, uint8_t txn_id,
                                      bool allow_all) noexcept;

    IServer& _server;
};

} // namespace server

/**
 * Server: server::AxisStore; SessionBank&lt;server::Session&gt; — у leaf.
 * Leaf-оси — обычно server::AxisBank (не этот store), либо свой storage().
 */
template <server::idx_t MaxAxes, uint8_t MaxSessions = kMaxConsoles>
class Server : public IServer {
public:
    static constexpr server::idx_t kMechCount = MaxAxes;
    static constexpr server::idx_t kAxisCount = MaxAxes;
    static constexpr uint8_t kSessionCount = MaxSessions;

    explicit Server(ILink& link, ClockFn clock) noexcept
        : IServer(link, clock)
    {}

protected:
    [[nodiscard]] server::AxisReg& storage() noexcept override { return _axes; }

    server::AxisStore<MaxAxes> _axes;
};

} // namespace segment
} // namespace smcp
