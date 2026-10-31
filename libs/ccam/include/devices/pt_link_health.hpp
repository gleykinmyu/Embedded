/**
 * @file pt_link_health.hpp
 * @brief Периодический опрос живности PT (#O) + оптики (#GZ) + опционально камеры (QID).
 *
 * Без RTOS: tick(now_ms). Канал PT/Cam — PtProbe на MISC::ReplyTimer.
 *
 * PH350 и др.: ответ #O может «залипать» на p2 после #O0; надёжный признак
 * нормально включённой головки — валидный hex от #GZ (при выключенной — gz---).
 * Не зависит от родной камеры / QID.
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/pt_device.hpp"
#include "ms_timer.hpp"
#include "transport/types.hpp"

#include <cstdint>
#include <cstring>

namespace ccam::devices {

/**
 * Один канал опроса: ReplyTimer (период + attempts) + alive/last.
 * OK → clear + start(period, waiting=false).
 * Fail → start(period, true); при isReplyLimit → alive=false.
 */
struct PtProbe {
    explicit PtProbe(uint8_t max_attempts = 3) noexcept
        : reply(max_attempts == 0 ? 1 : max_attempts)
    {
    }

    MISC::ReplyTimer reply;
    uint32_t interval_ms{1000};
    bool alive{false};
    Status last{Status::Timeout};
    uint32_t last_ok_ms{0};

    bool due(uint32_t now_ms) const
    {
        return !reply.isRunning() || reply.timedOut(now_ms);
    }

    void setIntervalMs(uint32_t ms, uint32_t min_ms)
    {
        interval_ms = ms < min_ms ? min_ms : ms;
    }

    void markOk(uint32_t now_ms)
    {
        alive = true;
        last_ok_ms = now_ms;
        reply.clear();
        reply.start(now_ms, interval_ms, false);
    }

    void markFail(uint32_t now_ms)
    {
        if (reply.isReplyLimit()) {
            alive = false;
            return;
        }
        reply.start(now_ms, interval_ms, true);
        if (reply.isReplyLimit()) {
            alive = false;
        }
    }

    void clear()
    {
        alive = false;
        last = Status::Timeout;
        last_ok_ms = 0;
        reply.clear();
    }

    /** Для UI: 0 пока alive, иначе attempts. */
    uint8_t failStreak() const { return alive ? 0 : reply.attempts(); }
};

class PtLinkHealth {
public:
    static constexpr uint32_t kDefaultIntervalMs = 1000;
    static constexpr uint32_t kDefaultCamIntervalMs = 3000;
    static constexpr uint8_t kDefaultFailLimit = 3;

    static constexpr uint8_t kPowerStarting = static_cast<uint8_t>(pt::PowerMode::Starting);
    static constexpr uint8_t kPowerOffPtControl = static_cast<uint8_t>(pt::PowerMode::OffPtControl);

    explicit PtLinkHealth(
        uint32_t interval_ms = kDefaultIntervalMs,
        uint8_t fail_limit = kDefaultFailLimit)
        : pt_(fail_limit == 0 ? 1 : fail_limit)
        , optics_(fail_limit == 0 ? 1 : fail_limit)
        , cam_(fail_limit == 0 ? 1 : fail_limit)
    {
        pt_.setIntervalMs(interval_ms, 200);
        optics_.setIntervalMs(interval_ms, 200);
        cam_.setIntervalMs(kDefaultCamIntervalMs, 500);
        cam_id_[0] = '\0';
    }

    void bind(PtDeviceBase* pt) { pt_dev_ = pt; }
    void bindCam(CameraDeviceBase* cam) { cam_dev_ = cam; }
    PtDeviceBase* device() const { return pt_dev_; }
    CameraDeviceBase* camera() const { return cam_dev_; }

    void setIntervalMs(uint32_t ms)
    {
        pt_.setIntervalMs(ms, 200);
        optics_.setIntervalMs(ms, 200);
    }
    uint32_t intervalMs() const { return pt_.interval_ms; }
    void setCamIntervalMs(uint32_t ms) { cam_.setIntervalMs(ms, 500); }

    const PtProbe& ptProbe() const { return pt_; }
    const PtProbe& opticsProbe() const { return optics_; }
    const PtProbe& camProbe() const { return cam_; }

    void tick(uint32_t now_ms);
    Status pollNow(uint32_t now_ms);

    bool alive() const { return pt_.alive; }
    /** Головка реально «на питании»: живой #O-линк и валидный #GZ (не ---). */
    bool powered() const { return pt_.alive && optics_.alive; }
    bool opticsAlive() const { return optics_.alive; }
    uint8_t powerCode() const { return power_code_; }
    const char* powerLabel() const;

    bool camTxLikely() const { return powered(); }
    bool camAlive() const { return cam_.alive; }
    const char* camId() const { return cam_id_; }
    uint8_t camFailStreak() const { return cam_.failStreak(); }
    uint8_t opticsFailStreak() const { return optics_.failStreak(); }

    uint8_t errorCode() const { return error_code_; }

    Status lastStatus() const { return pt_.last; }
    Status lastOpticsStatus() const { return optics_.last; }
    Status lastCamStatus() const { return cam_.last; }
    uint32_t lastOkMs() const { return pt_.last_ok_ms; }
    uint8_t failStreak() const { return pt_.failStreak(); }

private:
    Status doPoll(uint32_t now_ms);
    void pollOptics(uint32_t now_ms);
    void pollCam(uint32_t now_ms);
    void clearOptics();
    void clearCam();

    PtDeviceBase* pt_dev_{nullptr};
    CameraDeviceBase* cam_dev_{nullptr};

    PtProbe pt_;
    PtProbe optics_;
    PtProbe cam_;

    uint8_t power_code_{0xFF};
    uint8_t error_code_{0xFF};
    char cam_id_[24]{};
};

} // namespace ccam::devices
