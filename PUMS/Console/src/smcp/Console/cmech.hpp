/**
 * @file cmech.hpp
 * @brief CMech — IMech пульта: Select/Block/SetTarget TX через IConsole, Telemetry RX.
 */

#pragma once

#include <cstdint>

#include "obj_bank.hpp"
#include "smcp/mech.hpp"
#include "smcp/transport/message.hpp"

namespace smcp {

class IConsole;

/** IMech на пульте: Select/Block/SetTarget — TX; holder/status — только Telemetry. */
class CMech : public IMech {
public:
    CMech(IConsole& console, uint8_t id) noexcept;

    [[nodiscard]] IConsole& console() noexcept { return *_console; }
    [[nodiscard]] const IConsole& console() const noexcept { return *_console; }

    void select(uint8_t console_id) noexcept override;
    /** Сегментный Block на сервер (не GRUP / не локальный Status). */
    void block(bool blocked) noexcept override;
    void setTarget(const MotionTarget& target) noexcept override;
    void resetFault() noexcept override;

    /** Зеркало Telemetry (holder / status / position). */
    void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

private:
    IConsole* _console;
};

/** Банк CMech[N]: ctor регистрирует каждый в owner. */
template <uint8_t N>
using CMechBank = MISC::ObjBank<CMech, N>;

} // namespace smcp
