/**
 * @file link_hb.hpp
 * @brief FSM линка Session: Heartbeat + статус Idle/Connecting/Awaiting/Open.
 *
 * Владелец жизненного цикла линка. Session проксирует getStatus/isOpen.
 */

#pragma once

#include <cstdint>

#include "ms_timer.hpp"
#include "smcp/message.hpp"

namespace smcp {

class Session;

class LinkHb {
public:
    enum class Status : uint8_t {
        Idle = 0,
        Connecting,
        Awaiting,
        Open,
    };

    explicit LinkHb(Session& session) noexcept;

    [[nodiscard]] Status status() const noexcept { return _status; }
    [[nodiscard]] bool isOpen() const noexcept { return _status == Status::Open; }
    [[nodiscard]] bool isIdle() const noexcept { return _status == Status::Idle; }

    [[nodiscard]] static const char* cstr(Status status) noexcept;

    /** Session::open → Connecting. */
    void connect() noexcept;

    /** Session::close → Idle + сброс таймера. */
    void idle() noexcept;

    /** Connecting→ping; Open/Awaiting — miss / retry / HbLost. */
    void tick(uint32_t now_ms) noexcept;

    /** RX Heartbeat; иначе false. */
    [[nodiscard]] bool onPacket(const Packet& pkt, uint32_t now_ms) noexcept;

private:
    void setStatus(Status status) noexcept;
    void ping(uint32_t now_ms) noexcept;
    void pong() noexcept;

    Session& _s;
    Status _status = Status::Idle;
    MISC::ReplyTimer _hb{msg::Heartbeat::kMissMax};
};

} // namespace smcp
