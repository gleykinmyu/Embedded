#pragma once

#include "devices/ipt_device.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

/**
 * Состояние четырёх осей. Без mutex, очереди и задачи: вызывающий держит DeviceLock.
 * Последняя скорость не хранится: решение не сравнивает её с новой командой.
 * Кто запустил движение, не хранится: конец сессии применяется до новой команды.
 */
class MotionAxes {
public:
    enum class Mode : uint8_t { Idle = 0, Hold, Lease, StopPending };

    struct Slot {
        int64_t deadline_us{0};
        int64_t retry_after_us{0};
        Mode mode{Mode::Idle};
    };

    /** Не чаще одного автоматического STOP на ось. Явная команда интервал не ждёт. */
    static constexpr int64_t kStopRetryGapUs = 100000;

    using ClockUs = int64_t (*)();

    /**
     * lease_us == 0 — Hold. lease_us > 0 — Lease от момента успешного TX.
     * lease_us < 0 при ненулевой скорости — Param, состояние не меняется.
     * speed == 0 — STOP, срок не смотрится.
     * velocity_allowed == false — отказ новой скорости (OTA), STOP по-прежнему можно.
     * Пока общий STOP четырёх осей не завершён, новая скорость не отправляется.
     */
    ccam::Status drive(pt::IDevice &dev, ClockUs now, pt::Axis axis, int8_t speed, int64_t lease_us,
                       bool velocity_allowed = true);

    ccam::Status stopAxis(pt::IDevice &dev, ClockUs now, pt::Axis axis);
    ccam::Status stopAxes(pt::IDevice &dev, ClockUs now, const pt::Axis *axes, size_t count);

    /** Один проход: истёкший Lease и подошедший повтор StopPending. Без цикла до успеха. */
    void expire(pt::IDevice &dev, ClockUs now);

    /** Pan, tilt, zoom, focus через состояние, затем roll и iris как раньше. */
    ccam::Status stopLegacy(pt::IDevice &dev, ClockUs now);

    /**
     * Четыре оси перед home, recall, сменой модели, OTA и концом сессии.
     * Неполный успех оставляет общий STOP открытым: новая скорость, home и raw ждут.
     */
    ccam::Status haltTracked(pt::IDevice &dev, ClockUs now);

    bool globalStopOpen() const { return global_stop_open_; }

    const Slot &slot(pt::Axis axis) const;

    static bool tracked(pt::Axis axis);

private:
    static int indexOf(pt::Axis axis);
    ccam::Status stopOne(pt::IDevice &dev, ClockUs now, int index, bool automatic);
    void settleGlobal();

    Slot axes_[4]{};
    /** Обязательный STOP четырёх осей начат и ещё не доведён до конца. Не путать с StopPending одной оси. */
    bool global_stop_open_{false};
};
