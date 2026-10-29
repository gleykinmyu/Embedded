/**
 * @file pt_link_health.hpp
 * @brief Периодический опрос живности PT через #O (ответ p0..p3).
 *
 * Без RTOS: приложение вызывает tick(now_ms). Сам класс решает, пора ли
 * слать #O, и держит alive / power_code / fail streak.
 */

#pragma once

#include "devices/pt_device.hpp"
#include "transport/types.hpp"

#include <cstdint>

namespace ccam::devices {

class PtLinkHealth {
public:
    static constexpr uint32_t kDefaultIntervalMs = 1000;
    static constexpr uint8_t kDefaultFailLimit = 3;

    explicit PtLinkHealth(
        uint32_t interval_ms = kDefaultIntervalMs,
        uint8_t fail_limit = kDefaultFailLimit)
        : interval_ms_(interval_ms < 200 ? 200 : interval_ms)
        , fail_limit_(fail_limit == 0 ? 1 : fail_limit)
    {
    }

    void bind(PtDeviceBase* pt) { pt_ = pt; }
    PtDeviceBase* device() const { return pt_; }

    void setIntervalMs(uint32_t ms)
    {
        interval_ms_ = ms < 200 ? 200 : ms;
    }
    uint32_t intervalMs() const { return interval_ms_; }

    /**
     * Если прошло ≥ interval_ms с прошлого опроса — шлёт #O и обновляет
     * alive/power. Между опросами — no-op.
     */
    void tick(uint32_t now_ms);

    /** Принудительный #O вне расписания. */
    Status pollNow(uint32_t now_ms);

    bool alive() const { return alive_; }
    /** true если alive и power ≠ Off (p0). */
    bool powered() const { return alive_ && power_code_ != 0 && power_code_ != 0xFF; }
    /** Последний код pN (0..3), или 0xFF если неизвестен. */
    uint8_t powerCode() const { return power_code_; }
    Status lastStatus() const { return last_st_; }
    uint32_t lastOkMs() const { return last_ok_ms_; }
    uint8_t failStreak() const { return fails_; }

private:
    Status doPoll(uint32_t now_ms);

    PtDeviceBase* pt_{nullptr};
    uint32_t interval_ms_;
    uint8_t fail_limit_;
    uint32_t last_poll_ms_{0};
    uint32_t last_ok_ms_{0};
    uint8_t fails_{0};
    uint8_t power_code_{0xFF};
    bool alive_{false};
    Status last_st_{Status::Timeout};
};

} // namespace ccam::devices
