/**
 * @file cmech.hpp
 * @brief CMech — IAxis пульта: Select/Block/SetTarget TX через IConsole, Telemetry RX.
 */

#pragma once

#include <cstdint>

#include "obj_bank.hpp"
#include "smcp/mech/axis.hpp"
#include "smcp/mech/message.hpp"

namespace smcp {

class IConsole;

/** IAxis на пульте: Select/Block/SetTarget — TX; holder/status — только Telemetry. */
class CMech : public IAxis {
public:
    /** Один сегмент: сервер `msg::kServerIdMin`, local_id = axis_id. */
    CMech(IConsole& console, uint8_t id) noexcept;
    /** local_id = axis_id. Регистрация в storage по route. */
    CMech(IConsole& console, uint8_t server_id, uint8_t id) noexcept;
    /** Регистрация в storage(route.server_id) по route.axis_id. @a local_id — бит группы. */
    CMech(IConsole& console, uint8_t server_id, uint8_t axis_id, uint8_t local_id) noexcept;
    CMech(IConsole& console, axis::Route route, uint8_t local_id) noexcept;

    [[nodiscard]] IConsole& console() noexcept { return *_console; }
    [[nodiscard]] const IConsole& console() const noexcept { return *_console; }
    [[nodiscard]] axis::Route route() const noexcept { return _route; }
    [[nodiscard]] uint8_t serverId() const noexcept { return _route.server_id; }
    [[nodiscard]] uint8_t localId() const noexcept { return _local_id; }

    /** Select/Deselect этой оси (kHolderNone → Remove). TX всегда. */
    void select(uint8_t console_id) noexcept override;
    /** Сегментный Block на сервер (не GRUP / не локальный Status). */
    void block(bool blocked) noexcept override;
    /** Уставка этой оси через пульт. */
    void setTarget(const MotionTarget& target) noexcept override;
    /** ResetFault этой оси (бит axis_id). globalFault — ServerSession::resetFault({}). */
    void resetFault() noexcept override;

    /** Зеркало Telemetry (holder / status / position) с сервера. */
    void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

private:
    IConsole* _console;
    axis::Route _route{};
    uint8_t _local_id = 0;
};

/** Банк CMech[N]: ctor регистрирует каждый в owner. */
template <uint8_t N>
using CMechBank = MISC::ObjBank<CMech, N>;

} // namespace smcp
