/**
 * @file new_smcp/proto/link_hb.hpp
 * @brief Heartbeat / keep-alive сессии (master ping / slave miss).
 *
 * Idle → Connecting → Open; HbLost / Busy → Idle.
 * Пока ping ждёт ответ, это таймер (_hb.isWaiting), не отдельный статус.
 * start(true) — мастер (внешний Session::open): шлёт Request, пока не Open.
 * Request на Idle — сами становимся ведомым и отвечаем Ack.
 * После Open кадр пустой. ReplyTimer считает запуски.
 * Лимит kMissMax + 1: один запуск — окно после принятого HB.
 */

#pragma once

#include <cstdint>

#include "comm/old/container.hpp"
#include "ms_timer.hpp"
#include "smcp/new_smcp/transport.hpp"
#include "smcp/new_smcp/pdu.hpp"

namespace smcp {
class Session;
} // namespace smcp

namespace smcp::proto {

enum class SessionRole : uint8_t {
    Slave = 0,
    Master
};

/** Почему ушли в Idle. None — линк ещё не падал. */
enum class LinkStatus : uint8_t {
    Down = 0,
    Connecting,
    Up,
    LostHb,
    PeerBusy
};

class LinkHb : public comm::Proto<Frame> {
public:
    using Status = LinkStatus;
    using Role = SessionRole;

    /**
     * Лимит таймера — kMissMax + 1.
     * start() считает каждый запуск, а принятый HB сразу открывает окно тишины:
     * этот запуск и есть лишняя единица. Дальше остаётся kMissMax пустых окон.
     */
    explicit LinkHb(Session& session) noexcept;

    [[nodiscard]] Status status() const noexcept;
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] bool isMaster() const noexcept;

    /** true — мастер (Request). false — ведомый: ждём Request, на него отвечаем Ack. */
    void start() noexcept;

    /** Сброс в Idle (@a why ≠ None). */
    void stop() noexcept;

    /**
     * Чужой Heartbeat, кадр съедаем.
     * Мастер в Connecting: Busy гасит линк, Ack открывает. Сессию закрывает Session::onLink.
     * Мастер в Open и ведомый в Open фазу не режут.
     * Ведомый в Idle берёт только Request и на нём стартует. Connecting у ведомого нет.
     * Ждали свой ping — этот кадр и есть ответ, pong не шлём.
     * Не ждали — Pong: Ack на Request, иначе пустой KeepAlive.
     * Счётчик запусков обнуляется и тут же тратит 1 на окно тишины (waiting=false).
     */
    [[nodiscard]] bool onPacket(const Frame& pkt, MISC::time_t now) noexcept override;

    /**
     * Мастер: ping сразу, пока ответ не ждём, и снова по таймауту.
     * До Open фаза Request, после — пустой KeepAlive.
     * Молчание после своего ping при лимите — HbLost.
     * Тишина без ожидания (waiting=false) лимит не проверяет: это пауза до следующего ping.
     * Slave по таймауту ничего не шлёт, только продлевает тишину или падает по лимиту.
     */
    void tick(MISC::time_t now) noexcept override;

    void abort() noexcept override;

    [[nodiscard]] bool isDown() const noexcept;

protected:
    /** Ping и Pong — один Heartbeat на peer. Кто из них, помнит _wire до onTxDone. */
    [[nodiscard]] bool peekTx(Frame& out, MISC::time_t) noexcept override;

    /**
     * Pong таймер не трогает: мы никого не ждём.
     * Ping взводит ожидание ответа (waiting=true). Статус при этом не меняется.
     * Если пока кадр был в полёте уже пришёл HB, окно тишины живое — не затираем его.
     */
    void onTxDone(const Frame&, bool ok) noexcept override;

private:
    /**
     * Сначала гасим таймер и очередь, потом setStatus.
     * onLink синхронный: из него можно снова start(), и этот запуск reset уже не затрёт.
     */
    void reset(Status st) noexcept;

    void prepareTxMsg(Heartbeat::Msg msg) noexcept;

    /** Смена статуса и колбэк сессии только на ребре Open / уход из живого линка. */
    void setStatus(Status st) noexcept;

    enum class txState : uint8_t {
        Idle = 0,
        Ready,
        Packed,
        Sent,
    };

    Session& _session;
    Status _status = Status::Down;
    Role _role = Role::Slave;
    Heartbeat::Msg _txMsg = Heartbeat::Msg::KeepAlive;
    txState _txState = txState::Idle;
    MISC::ReplyTimer _timer; ///< запуски окон; waiting — ждём ответ на ping
};

} // namespace smcp::proto
