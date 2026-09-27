/**
 * @file cmech.hpp
 * @brief CMech — IMech пульта: Select/Block/SetTarget TX через IConsole, Telemetry RX.
 */

#pragma once

#include <cstdint>

#include "obj_bank.hpp"
#include "smcp/mech/mech.hpp"
#include "smcp/mech/message.hpp"

namespace smcp {

class IConsole;

/** IMech на пульте: Select/Block/SetTarget — TX; holder/status — только Telemetry. */
class CMech : public IMech {
public:
    /** 0 — не id на шине: primary-банк (`storage()`), пока один сегмент. */
    static constexpr uint8_t kPrimaryServer = 0;

    /** `segment(kPrimaryServer)` → `storage()`. */
    CMech(IConsole& console, uint8_t id) noexcept;
    /**
     * `segment(server_id)`. Не 0 и нет банка в `segment()` — RegisterFailed.
     */
    CMech(IConsole& console, uint8_t server_id, uint8_t id) noexcept;

    [[nodiscard]] IConsole& console() noexcept { return *_console; }
    [[nodiscard]] const IConsole& console() const noexcept { return *_console; }
    [[nodiscard]] uint8_t serverId() const noexcept { return _server_id; }

    /** Select/Deselect этой оси (kHolderNone → Remove). TX всегда. */
    void select(uint8_t console_id) noexcept override;
    /** Сегментный Block на сервер (не GRUP / не локальный Status). */
    void block(bool blocked) noexcept override;
    /** Уставка этой оси через пульт. */
    void setTarget(const MotionTarget& target) noexcept override;
    /** На пульте нет локального сброса: команда уходит с сервера. */
    void resetFault() noexcept override;

    /** Зеркало Telemetry (holder / status / position) с сервера. */
    void onTelemetry(uint8_t src_id, const msg::Telemetry& telemetry) noexcept;

private:
    IConsole* _console;
    uint8_t _server_id = kPrimaryServer;
};

/** Банк CMech[N]: ctor регистрирует каждый в owner. */
template <uint8_t N>
using CMechBank = MISC::ObjBank<CMech, N>;

} // namespace smcp
