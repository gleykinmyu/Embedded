#pragma once

/**
 * @file nexTimeout.hpp
 *
 * Одноразовый timeout по `uint32_t` ms (wrap-safe).
 * `_expires_ms == 0` — остановлен (`kStopped`).
 */

#include <cstdint>

namespace nex {

class MsTimer {
public:
    static constexpr uint32_t kStopped = 0u;

    constexpr MsTimer() noexcept = default;

    [[nodiscard]] constexpr bool isRunning() const noexcept { return _expires_ms != kStopped; }

    void stop() noexcept { _expires_ms = kStopped; }

    /** `_expires_ms = now_ms + duration_ms`. */
    void start(uint32_t now_ms, uint32_t duration_ms) noexcept { _expires_ms = now_ms + duration_ms; }

    /** Start только если stopped. */
    void startOnce(uint32_t now_ms, uint32_t duration_ms) noexcept {
        if (!isRunning())
            start(now_ms, duration_ms);
    }

    /** true — timeout истёк (полу-кольцо uint32_t). */
    [[nodiscard]] bool timedOut(uint32_t now_ms) const noexcept {
        if (!isRunning())
            return false;
        constexpr uint32_t kHalfModulus = UINT32_C(1) << 31;
        return (now_ms - _expires_ms) < kHalfModulus;
    }

private:
    uint32_t _expires_ms = kStopped;
};

} // namespace nex
