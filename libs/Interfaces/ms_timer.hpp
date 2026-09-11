/**
 * @file ms_timer.hpp
 * @brief Одноразовый таймаут по `uint32_t` ms (wrap-safe) и ReplyTimer (попытки + waiting).
 *
 * `_expires_ms == 0` — остановлен (`kStopped`).
 */

#pragma once

#include <cstdint>

namespace MISC {

class MsTimer {
public:
    static constexpr uint32_t kStopped = 0u;

    constexpr MsTimer() noexcept = default;

    [[nodiscard]] constexpr bool isRunning() const noexcept { return _expires_ms != kStopped; }

    void stop() noexcept { _expires_ms = kStopped; }

    /** `_expires_ms = now_ms + duration_ms`. */
    void start(uint32_t now_ms, uint32_t duration_ms) noexcept { _expires_ms = now_ms + duration_ms; }

    /** Start только если stopped. */
    void startOnce(uint32_t now_ms, uint32_t duration_ms) noexcept
    {
        if (!isRunning()) {
            start(now_ms, duration_ms);
        }
    }

    /** true — timeout истёк (полу-кольцо uint32_t). */
    [[nodiscard]] bool timedOut(uint32_t now_ms) const noexcept
    {
        if (!isRunning()) {
            return false;
        }
        constexpr uint32_t kHalfModulus = UINT32_C(1) << 31;
        return (now_ms - _expires_ms) < kHalfModulus;
    }

private:
    uint32_t _expires_ms = kStopped;
};

/**
 * MsTimer + лимит попыток + флаг «ждём ответ».
 * start → ++attempts + T (+ isWaiting по умолчанию).
 * Первое окно: clear + start(..., false); miss/retry: сначала isReplyLimit, потом start.
 */
class ReplyTimer {
public:
    explicit ReplyTimer(uint8_t max_attempts) noexcept
        : _max_attempts(max_attempts)
    {}

    void clear() noexcept
    {
        _waiting = false;
        _attempts = 0;
        _timer.stop();
    }

    [[nodiscard]] bool isWaiting() const noexcept { return _waiting; }
    [[nodiscard]] uint8_t attempts() const noexcept { return _attempts; }
    [[nodiscard]] bool isRunning() const noexcept { return _timer.isRunning(); }

    /** ++attempts, старт T; waiting — ждать ответ (false = окно тишины / miss). */
    void start(uint32_t now_ms, uint32_t timeout_ms, bool waiting = true) noexcept
    {
        ++_attempts;
        _waiting = waiting;
        _timer.start(now_ms, timeout_ms);
    }

    [[nodiscard]] bool timedOut(uint32_t now_ms) const noexcept
    {
        return _timer.timedOut(now_ms);
    }

    [[nodiscard]] bool isReplyLimit() const noexcept
    {
        return _attempts >= _max_attempts;
    }

private:
    MsTimer _timer{};
    uint8_t _max_attempts = 1;
    uint8_t _attempts = 0;
    bool _waiting = false;
};

} // namespace MISC
