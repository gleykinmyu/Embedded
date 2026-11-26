#include "motion_axes.hpp"

static_assert(sizeof(MotionAxes::Slot) == 24, "Axis slot is 24 bytes");
static_assert(sizeof(MotionAxes) == 104, "Four slots and the global-stop flag are 104 bytes");

namespace {

bool speedOk(int8_t speed)
{
    return speed >= -100 && speed <= 100;
}

bool transmitted(ccam::Status st)
{
    return st == ccam::Status::Io || st == ccam::Status::Timeout;
}

} // namespace

bool MotionAxes::tracked(pt::Axis axis)
{
    return indexOf(axis) >= 0;
}

int MotionAxes::indexOf(pt::Axis axis)
{
    switch (axis) {
    case pt::Axis::Pan:
        return 0;
    case pt::Axis::Tilt:
        return 1;
    case pt::Axis::Zoom:
        return 2;
    case pt::Axis::Focus:
        return 3;
    default:
        return -1;
    }
}

const MotionAxes::Slot &MotionAxes::slot(pt::Axis axis) const
{
    static const Slot kEmpty{};
    const int index = indexOf(axis);
    if (index < 0) {
        return kEmpty;
    }
    return axes_[index];
}

void MotionAxes::settleGlobal()
{
    if (!global_stop_open_) {
        return;
    }
    for (const Slot &slot : axes_) {
        if (slot.mode == Mode::StopPending) {
            return;
        }
    }
    global_stop_open_ = false;
}

ccam::Status MotionAxes::stopOne(pt::IDevice &dev, ClockUs now, int index, bool automatic)
{
    Slot &slot = axes_[index];
    const int64_t at = now != nullptr ? now() : 0;
    if (automatic) {
        const bool lease_due = slot.mode == Mode::Lease && at >= slot.deadline_us;
        const bool retry_due = slot.mode == Mode::StopPending && at >= slot.retry_after_us;
        if (!lease_due && !retry_due) {
            return ccam::Status::Ok;
        }
    }

    static constexpr pt::Axis kByIndex[] = {
        pt::Axis::Pan, pt::Axis::Tilt, pt::Axis::Zoom, pt::Axis::Focus,
    };
    const ccam::Status st = dev.move(kByIndex[index], 0);
    if (st == ccam::Status::Ok) {
        slot = Slot{};
        settleGlobal();
        return ccam::Status::Ok;
    }
    /* Param и Buffer возникают до UART. Idle не доказывает, что ось стоит: raw обходит слот. */
    slot.mode = Mode::StopPending;
    slot.deadline_us = 0;
    const int64_t after = now != nullptr ? now() : at;
    if (after > INT64_MAX - kStopRetryGapUs) {
        slot.retry_after_us = INT64_MAX;
    } else {
        slot.retry_after_us = after + kStopRetryGapUs;
    }
    return st;
}

ccam::Status MotionAxes::drive(pt::IDevice &dev, ClockUs now, pt::Axis axis, int8_t speed,
                               int64_t lease_us, bool velocity_allowed)
{
    const int index = indexOf(axis);
    if (index < 0 || !speedOk(speed)) {
        return ccam::Status::Param;
    }
    if (speed == 0) {
        return stopOne(dev, now, index, false);
    }
    if (lease_us < 0) {
        return ccam::Status::Param;
    }
    if (!velocity_allowed || global_stop_open_) {
        return ccam::Status::Io;
    }

    Slot &slot = axes_[index];
    const int64_t at = now != nullptr ? now() : 0;
    const bool must_stop = slot.mode == Mode::StopPending ||
                           (slot.mode == Mode::Lease && at >= slot.deadline_us);
    if (must_stop) {
        const ccam::Status stopped = stopOne(dev, now, index, false);
        if (stopped != ccam::Status::Ok) {
            return stopped;
        }
    }

    const ccam::Status st = dev.move(axis, speed);
    if (st != ccam::Status::Ok) {
        if (transmitted(st)) {
            slot.mode = Mode::StopPending;
            slot.deadline_us = 0;
            const int64_t after = now != nullptr ? now() : at;
            if (after > INT64_MAX - kStopRetryGapUs) {
                slot.retry_after_us = INT64_MAX;
            } else {
                slot.retry_after_us = after + kStopRetryGapUs;
            }
        }
        return st;
    }

    const int64_t done = now != nullptr ? now() : at;
    slot.retry_after_us = 0;
    if (lease_us == 0) {
        slot.mode = Mode::Hold;
        slot.deadline_us = 0;
        return ccam::Status::Ok;
    }
    slot.mode = Mode::Lease;
    if (lease_us > 0 && done > INT64_MAX - lease_us) {
        slot.deadline_us = INT64_MAX;
    } else {
        slot.deadline_us = done + lease_us;
    }
    return ccam::Status::Ok;
}

ccam::Status MotionAxes::stopAxis(pt::IDevice &dev, ClockUs now, pt::Axis axis)
{
    const int index = indexOf(axis);
    if (index < 0) {
        return ccam::Status::Param;
    }
    return stopOne(dev, now, index, false);
}

ccam::Status MotionAxes::stopAxes(pt::IDevice &dev, ClockUs now, const pt::Axis *axes, size_t count)
{
    if (count > 0 && axes == nullptr) {
        return ccam::Status::Param;
    }
    ccam::Status first = ccam::Status::Ok;
    for (size_t i = 0; i < count; ++i) {
        const ccam::Status st = stopAxis(dev, now, axes[i]);
        if (st != ccam::Status::Ok && first == ccam::Status::Ok) {
            first = st;
        }
    }
    return first;
}

void MotionAxes::expire(pt::IDevice &dev, ClockUs now)
{
    for (int i = 0; i < 4; ++i) {
        (void)stopOne(dev, now, i, true);
    }
}

ccam::Status MotionAxes::haltTracked(pt::IDevice &dev, ClockUs now)
{
    static constexpr pt::Axis kAxes[] = {
        pt::Axis::Pan, pt::Axis::Tilt, pt::Axis::Zoom, pt::Axis::Focus,
    };
    const ccam::Status st = stopAxes(dev, now, kAxes, 4);
    global_stop_open_ = st != ccam::Status::Ok;
    return st;
}

ccam::Status MotionAxes::stopLegacy(pt::IDevice &dev, ClockUs now)
{
    ccam::Status first = haltTracked(dev, now);
    const ccam::Status roll = dev.move(pt::Axis::Roll, 0);
    if (roll != ccam::Status::Ok && first == ccam::Status::Ok) {
        first = roll;
    }
    const ccam::Status iris = dev.move(pt::Axis::Iris, 0);
    if (iris != ccam::Status::Ok && first == ccam::Status::Ok) {
        first = iris;
    }
    return first;
}
