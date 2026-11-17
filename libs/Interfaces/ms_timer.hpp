/**
 * @file ms_timer.hpp
 * @brief Одноразовый таймаут по time_t (мс, wrap-safe) и ReplyTimer (попытки + waiting).
 *
 * `_expires_ms == 0` — остановлен (`kStopped`): обнулённый объект = остановлен.
 */

#pragma once

#include <cstdint>

namespace MISC {

/** Момент и длительность в миллисекундах. Ширина — у таймера, снаружи не выбирается. */
using time_t = uint32_t;

class MsTimer {
public:
    static constexpr time_t kStopped = 0u;

    constexpr MsTimer() noexcept = default;

    [[nodiscard]] constexpr bool isRunning() const noexcept { return _expires_ms != kStopped; }

    void stop() noexcept { _expires_ms = kStopped; }

    /** `_expires_ms = now_ms + duration_ms`; попадание на kStopped → +1 мс. */
    void start(time_t now_ms, time_t duration_ms) noexcept
    {
        const time_t t = now_ms + duration_ms;
        _expires_ms = (t == kStopped) ? t + 1u : t;
    }

    /** Start только если stopped. */
    void startOnce(time_t now_ms, time_t duration_ms) noexcept
    {
        if (!isRunning()) {
            start(now_ms, duration_ms);
        }
    }

    /** true — timeout истёк (полу-кольцо time_t). */
    [[nodiscard]] bool timedOut(time_t now_ms) const noexcept
    {
        if (!isRunning()) {
            return false;
        }
        constexpr time_t kHalfModulus = time_t{1} << 31;
        return (now_ms - _expires_ms) < kHalfModulus;
    }

private:
    time_t _expires_ms = kStopped;
};

/**
 * MsTimer + лимит попыток + флаг «ждём ответ».
 * start → ++attempts + T (+ isWaiting по умолчанию).
 * Первое окно: clear + start(..., false); miss/retry: сначала isAttemptsOver, потом start.
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

    /** Ждём ответ. Пока таймер не запущен, ожидания нет. */

    //Таймер запущен пока ждем ответ на свой запрос. Если нужно окно тишины, то запускаем таймер с waiting = false.
    [[nodiscard]] bool isWaiting() const noexcept { return _waiting && _timer.isRunning(); }
    [[nodiscard]] uint8_t attempts() const noexcept { return _attempts; }
    [[nodiscard]] bool isRunning() const noexcept { return _timer.isRunning(); }

    /** ++attempts, старт T; waiting — ждать ответ (false = окно тишины / miss). */
    void start(time_t now_ms, time_t timeout_ms, bool waiting = true) noexcept
    {
        ++_attempts;
        _waiting = waiting;
        _timer.start(now_ms, timeout_ms);
    }

    /** Продлить T без ++attempts (например multi-byte тело, пока ждём Ack). */
    void extend(time_t now_ms, time_t timeout_ms) noexcept
    {
        if (_waiting) {
            _timer.start(now_ms, timeout_ms);
        }
    }

    [[nodiscard]] bool timedOut(time_t now_ms) const noexcept
    {
        return _timer.timedOut(now_ms);
    }

    [[nodiscard]] bool isAttemptsOver() const noexcept
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
