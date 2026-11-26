#include "devices/pt_device.hpp"
#include "motion_axes.hpp"
#include "session_end.hpp"
#include "transport/transport.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

int g_fails = 0;
uint32_t g_ms = 0;
int64_t g_us = 0;

void expect(bool cond, const char *name)
{
    if (!cond) {
        std::printf("FAIL %s\n", name);
        ++g_fails;
    } else {
        std::printf("PASS %s\n", name);
    }
}

uint32_t now_ms()
{
    return g_ms;
}

int64_t now_us()
{
    return g_us;
}

class DepthLock final : public BIF::ILockable {
protected:
    void lock() override {}
    void unlock() override {}
};

class TestHead final : public ccam::devices::PtDeviceBase {
public:
    using PtDeviceBase::PtDeviceBase;
    pt::Status power(pt::PowerMode) override { return pt::Status::Param; }
};

class FocusBlind final : public ccam::devices::PtDeviceBase {
public:
    using PtDeviceBase::PtDeviceBase;
    pt::Status power(pt::PowerMode) override { return pt::Status::Param; }
    ccam::Status move(pt::Axis axis, int8_t speed) override
    {
        if (axis == pt::Axis::Focus) {
            return ccam::Status::Param;
        }
        return PtDeviceBase::move(axis, speed);
    }
};

class ScriptStream final : public BIF::IByteStream {
public:
    int fail_write_on{0};
    int fail_write_after{0};
    int64_t advance_us{0};
    int writes{0};

    size_t write(const uint8_t *, size_t size) override
    {
        ++writes;
        g_us += advance_us;
        if (fail_write_on > 0 && writes == fail_write_on) {
            g_ms += 200;
            return 0;
        }
        if (fail_write_after > 0 && writes >= fail_write_after) {
            g_ms += 200;
            return 0;
        }
        return size;
    }
    size_t read(uint8_t *, size_t) override { return 0; }
    size_t available() const override { return 0; }
    size_t availableForWrite() const override { return 64; }
    void purge() override {}
    void purgeOutput() override {}
    bool flushTx() override { return true; }
    void flush() override {}
    bool open(uint32_t) override { return true; }
    void close() override {}
    bool isOpen() override { return true; }
    Status getStatus() override { return Status::OK; }
    void clearErrors() override {}
};

struct Rig {
    DepthLock lock;
    ScriptStream stream;
    ccam::Transport bus;
    TestHead head;
    MotionAxes motion;

    Rig() : bus(stream, &now_ms, lock), head(bus) {}
};

void reset_clock()
{
    g_ms = 0;
    g_us = 1000000;
}

void test_initial_and_hold()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "pan starts idle");
    expect(rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::Idle, "tilt starts idle");
    expect(rig.motion.slot(pt::Axis::Zoom).mode == MotionAxes::Mode::Idle, "zoom starts idle");
    expect(rig.motion.slot(pt::Axis::Focus).mode == MotionAxes::Mode::Idle, "focus starts idle");
    expect(sizeof(MotionAxes) == 104, "four slots and the global-stop flag use 104 bytes");

    const auto st = rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 40, 0);
    expect(st == ccam::Status::Ok, "hold drive ok");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Hold, "hold has no deadline mode");
    expect(rig.motion.slot(pt::Axis::Pan).deadline_us == 0, "hold deadline is zero");
    expect(rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::Idle, "tilt stays idle");
    const int writes = rig.stream.writes;
    g_us += 60LL * 1000000LL;
    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == writes, "hold is not stopped by expire");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Hold, "hold remains");
}

void test_independent_and_resend()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 0) == ccam::Status::Ok, "pan");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Tilt, -20, 0) == ccam::Status::Ok, "tilt");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Zoom, 30, 0) == ccam::Status::Ok, "zoom");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Focus, 40, 0) == ccam::Status::Ok, "focus");
    expect(rig.stream.writes == 4, "four axes are four frames");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 0) == ccam::Status::Ok, "same speed sent again");
    expect(rig.stream.writes == 5, "unchanged speed is not skipped");
}

void test_lease_deadline_and_failure()
{
    reset_clock();
    Rig rig;
    rig.stream.advance_us = 7000;
    constexpr int64_t lease = 800000;
    const int64_t before = g_us;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Zoom, 25, lease) == ccam::Status::Ok, "lease created");
    const auto &zoom = rig.motion.slot(pt::Axis::Zoom);
    expect(zoom.mode == MotionAxes::Mode::Lease, "lease mode");
    expect(zoom.deadline_us == before + 7000 + lease, "deadline starts when tx completes");
    const int64_t deadline = zoom.deadline_us;
    const int writes = rig.stream.writes;

    g_us = deadline - 1;
    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == writes, "before deadline there is no stop");
    expect(rig.motion.slot(pt::Axis::Zoom).mode == MotionAxes::Mode::Lease, "lease still active");

    g_us = deadline;
    rig.motion.expire(rig.head, &now_us);
    expect(rig.motion.slot(pt::Axis::Zoom).mode == MotionAxes::Mode::Idle, "now == deadline stops the axis");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "other axes stay untouched");

    reset_clock();
    rig.stream.writes = 0;
    rig.stream.advance_us = 0;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 15, lease) == ccam::Status::Ok, "lease to extend");
    const int64_t kept = rig.motion.slot(pt::Axis::Pan).deadline_us;
    rig.stream.fail_write_after = 1;
    const auto again = rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 15, lease);
    expect(again == ccam::Status::Io, "failed resend is not ok");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::StopPending, "failed velocity becomes stop-pending");
    expect(rig.motion.slot(pt::Axis::Pan).deadline_us == 0, "failed resend does not extend the lease");
    expect(rig.motion.slot(pt::Axis::Pan).deadline_us != kept, "old deadline is not kept as a live lease");
}

void test_validation_and_first_failure()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Iris, 10, 0) == ccam::Status::Param, "iris is not a tracked axis");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 101, 0) == ccam::Status::Param, "speed above 100");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, -1) == ccam::Status::Param, "negative lease");
    expect(rig.stream.writes == 0, "invalid drive does not touch the bus");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "invalid drive keeps idle");

    rig.stream.fail_write_after = 1;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Tilt, 10, 0) == ccam::Status::Io, "first velocity from idle can fail");
    expect(rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::StopPending, "indeterminate first tx is stop-pending");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "failed tilt does not touch pan");
}

void test_stop_pending_and_retry_gap()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 0) == ccam::Status::Ok, "arm pan");
    rig.stream.fail_write_on = rig.stream.writes + 1;
    expect(rig.motion.stopAxis(rig.head, &now_us, pt::Axis::Pan) == ccam::Status::Io, "stop failure");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::StopPending, "failed stop is stop-pending");
    const int after_fail = rig.stream.writes;

    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == after_fail, "retry waits out the gap");
    g_us += MotionAxes::kStopRetryGapUs - 1;
    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == after_fail, "one microsecond early is still inside the gap");
    g_us += 1;
    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == after_fail + 1, "one automatic retry after the gap");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "retry stop succeeded");

    rig.stream.fail_write_after = rig.stream.writes + 1;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 0) == ccam::Status::Io, "back to stop-pending");
    const int pending_writes = rig.stream.writes;
    const auto refused = rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 33, 0);
    expect(refused != ccam::Status::Ok, "velocity during stop-pending needs a successful stop");
    expect(rig.stream.writes == pending_writes + 1, "only the stop was attempted");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::StopPending, "still stop-pending");

    rig.stream.fail_write_after = 0;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 33, 500000) == ccam::Status::Ok, "velocity after stop");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Lease, "new lease after the stop");
}

void test_expired_lease_then_new_command()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Focus, 12, 5000) == ccam::Status::Ok, "short lease");
    g_us = rig.motion.slot(pt::Axis::Focus).deadline_us;
    const int before = rig.stream.writes;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Focus, 18, 0) == ccam::Status::Ok, "new command at expiry");
    expect(rig.stream.writes == before + 2, "stop and then the new velocity");
    expect(rig.motion.slot(pt::Axis::Focus).mode == MotionAxes::Mode::Hold, "expired lease is not extended in place");
}

void test_stop_axes_and_legacy()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 0) == ccam::Status::Ok, "pan hold");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Tilt, 10, 0) == ccam::Status::Ok, "tilt hold");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Zoom, 10, 0) == ccam::Status::Ok, "zoom hold");
    static constexpr pt::Axis kThree[] = {pt::Axis::Pan, pt::Axis::Tilt, pt::Axis::Zoom};
    rig.stream.fail_write_on = rig.stream.writes + 2;
    const auto st = rig.motion.stopAxes(rig.head, &now_us, kThree, 3);
    expect(st == ccam::Status::Io, "stopAxes returns the first error");
    expect(rig.stream.writes == 6, "later axes are still stopped");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "pan stopped");
    expect(rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::StopPending, "tilt remains pending");
    expect(rig.motion.slot(pt::Axis::Zoom).mode == MotionAxes::Mode::Idle, "zoom stopped after the error");

    rig.stream.fail_write_on = 0;
    rig.stream.writes = 0;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 5, 0) == ccam::Status::Ok, "rearm");
    rig.stream.fail_write_on = rig.stream.writes + 1;
    const auto legacy = rig.motion.stopLegacy(rig.head, &now_us);
    expect(legacy == ccam::Status::Io, "legacy stop reports the first error");
    expect(rig.stream.writes == 7, "legacy stop attempts six axes");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::StopPending, "tracked axis remembers the failed stop");
}

MotionGateResult pass_gate(SessionEndFlag &ends, MotionAxes &motion, ccam::devices::PtDeviceBase &head,
                           int &halts, bool ota, bool admit_ok, bool note_during_halt)
{
    return gateMotion(
        ota, motion.globalStopOpen(), [&] { return ends.take(); },
        [&] {
            ++halts;
            if (note_during_halt) {
                ends.note();
            }
            return motion.haltTracked(head, &now_us);
        },
        [&] { return ends.isPending(); }, [&] { return admit_ok; });
}

void test_session_generation()
{
    reset_clock();
    Rig rig;
    SessionEndFlag ends;
    int halts = 0;
    ends.note();
    ends.note();
    auto first = pass_gate(ends, rig.motion, rig.head, halts, false, true, false);
    expect(first.gate == MotionGate::Proceed, "one pending end is applied once");
    expect(halts == 1, "several ends before the gate produce one stop");
    expect(!ends.isPending(), "the flag is consumed");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 20, 0) == ccam::Status::Ok, "first command after the end");
    auto second = pass_gate(ends, rig.motion, rig.head, halts, false, true, false);
    expect(second.gate == MotionGate::Proceed, "the next command sees no pending end");
    expect(halts == 1, "the same end does not stop the later command");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Tilt, 20, 0) == ccam::Status::Ok, "second command after the same end");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Hold, "pan admitted after the end keeps moving");
    expect(rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::Hold, "tilt admitted after the end keeps moving");

    ends.note();
    auto replaced = pass_gate(ends, rig.motion, rig.head, halts, false, true, false);
    expect(replaced.gate == MotionGate::Proceed, "a new operator session is admitted after the stop");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "motion from the ended session is stopped");
    expect(rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::Idle, "both axes of the ended session are stopped");
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 12, 0) == ccam::Status::Ok, "the new session can move");

    ends.note();
    int late_halts = 0;
    auto late = gateMotion(
        false, rig.motion.globalStopOpen(),
        [&] {
            const bool had = ends.take();
            ends.note();
            return had;
        },
        [&] {
            ++late_halts;
            return rig.motion.haltTracked(rig.head, &now_us);
        },
        [&] { return ends.isPending(); }, [&] { return true; });
    expect(late.gate == MotionGate::Superseded, "an end between the flag and admission rejects the command");
    expect(late_halts == 1, "that end does not start a second stop loop");
    expect(ends.isPending(), "the newer end stays pending");
    const int before_reject = rig.stream.writes;
    expect(pass_gate(ends, rig.motion, rig.head, late_halts, false, true, false).gate == MotionGate::Proceed,
           "the following pass applies the newer end");
    expect(rig.stream.writes > before_reject, "the newer end still stops the axes");
}

void test_preliminary_and_unsupported()
{
    reset_clock();
    Rig rig;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 500000) == ccam::Status::Ok, "lease before operations");
    const int64_t deadline = rig.motion.slot(pt::Axis::Pan).deadline_us;
    const int writes = rig.stream.writes;
    g_us += 10;
    expect(deadline == rig.motion.slot(pt::Axis::Pan).deadline_us, "clock alone does not change a lease");
    expect(rig.stream.writes == writes, "preset save and default rate send nothing here");

    static constexpr pt::Axis kAim[] = {pt::Axis::Pan, pt::Axis::Tilt};
    expect(rig.motion.stopAxes(rig.head, &now_us, kAim, 2) == ccam::Status::Ok, "aim stop");
    bool aim_started = false;
    if (rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle &&
        rig.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::Idle) {
        aim_started = true;
    }
    expect(aim_started, "absolute aim starts only after both axes are idle");

    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Zoom, 10, 0) == ccam::Status::Ok, "zoom hold");
    rig.stream.fail_write_after = rig.stream.writes + 1;
    const auto halted = rig.motion.haltTracked(rig.head, &now_us);
    bool replaced = false;
    bool home_sent = false;
    if (halted == ccam::Status::Ok) {
        replaced = true;
        home_sent = true;
    }
    expect(!replaced, "model change does not proceed after a failed stop");
    expect(!home_sent, "home and recall do not proceed after a failed stop");
    expect(rig.motion.slot(pt::Axis::Zoom).mode == MotionAxes::Mode::StopPending, "failed halt leaves stop-pending");
    expect(rig.stream.writes == 8, "halt still tries all four axes");

    DepthLock lock;
    ScriptStream stream;
    ccam::Transport bus(stream, &now_ms, lock);
    FocusBlind blind(bus);
    MotionAxes motion;
    expect(motion.haltTracked(blind, &now_us) == ccam::Status::Param, "param on an idle stop is still a failure");
    expect(stream.writes == 3, "param is returned before a byte is written");
    expect(motion.slot(pt::Axis::Focus).mode == MotionAxes::Mode::StopPending, "failed idle stop stays pending");
    expect(motion.globalStopOpen(), "a failed four-axis stop stays open");

    expect(motion.drive(blind, &now_us, pt::Axis::Pan, 10, 0, false) == ccam::Status::Io, "ota blocks new velocity");
    expect(motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "ota does not change the axis");
    expect(stream.writes == 3, "ota sends no velocity frame");
}

void test_retry_is_bounded()
{
    reset_clock();
    Rig rig;
    rig.stream.fail_write_after = 1;
    static constexpr pt::Axis kFour[] = {
        pt::Axis::Pan, pt::Axis::Tilt, pt::Axis::Zoom, pt::Axis::Focus,
    };
    for (pt::Axis axis : kFour) {
        (void)rig.motion.drive(rig.head, &now_us, axis, 10, 0);
    }
    expect(rig.stream.writes == 4, "four failing drives");
    const int stuck = rig.stream.writes;
    rig.motion.expire(rig.head, &now_us);
    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == stuck, "a second expire inside the gap sends nothing");
    g_us += MotionAxes::kStopRetryGapUs;
    rig.motion.expire(rig.head, &now_us);
    expect(rig.stream.writes == stuck + 4, "one retry per axis, not a tight loop");
}

bool source_has(const char *path, const char *needle)
{
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str().find(needle) != std::string::npos;
}

class ForcedHead final : public ccam::devices::PtDeviceBase {
public:
    using PtDeviceBase::PtDeviceBase;
    pt::Status power(pt::PowerMode) override { return pt::Status::Param; }
    ccam::Status forced{ccam::Status::Ok};
    bool use_forced{false};

    ccam::Status move(pt::Axis axis, int8_t speed) override
    {
        if (use_forced) {
            return forced;
        }
        return PtDeviceBase::move(axis, speed);
    }
};

void test_pre_tx_and_owner_kept()
{
    reset_clock();
    DepthLock lock;
    ScriptStream stream;
    ccam::Transport bus(stream, &now_ms, lock);
    ForcedHead head(bus);
    MotionAxes motion;
    head.use_forced = true;
    head.forced = ccam::Status::Param;
    expect(motion.stopAxis(head, &now_us, pt::Axis::Pan) == ccam::Status::Param, "idle param stop is a failure");
    expect(stream.writes == 0, "param happens before a byte is written");
    expect(motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::StopPending, "idle param keeps the stop duty");
    head.forced = ccam::Status::Buffer;
    expect(motion.stopAxis(head, &now_us, pt::Axis::Tilt) == ccam::Status::Buffer, "idle buffer stop is a failure");
    expect(stream.writes == 0, "buffer happens before a byte is written");
    expect(motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::StopPending, "idle buffer keeps the stop duty");
    head.forced = ccam::Status::Param;
    expect(motion.haltTracked(head, &now_us) == ccam::Status::Param, "a mandatory halt reports the first error");
    expect(motion.globalStopOpen(), "param and buffer keep the global stop open");

    head.use_forced = false;
    head.forced = ccam::Status::Ok;
    expect(motion.haltTracked(head, &now_us) == ccam::Status::Ok, "the retry of the global stop can succeed");
    expect(!motion.globalStopOpen(), "a successful global stop closes the duty");
    const auto &cleared = motion.slot(pt::Axis::Pan);
    expect(cleared.mode == MotionAxes::Mode::Idle, "successful stop is idle");
    expect(cleared.deadline_us == 0 && cleared.retry_after_us == 0, "successful stop clears the slot");
}

void test_lease_sum_and_wrap()
{
    reset_clock();
    Rig rig;
    g_us = INT64_MAX - 5;
    expect(rig.motion.drive(rig.head, &now_us, pt::Axis::Pan, 10, 100) == ccam::Status::Ok, "lease near the time limit");
    expect(rig.motion.slot(pt::Axis::Pan).deadline_us == INT64_MAX, "lease sum saturates");
    expect(rig.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Lease, "saturated deadline is still a lease");

    reset_clock();
    Rig idle;
    const int before = idle.stream.writes;
    expect(idle.motion.stopLegacy(idle.head, &now_us) == ccam::Status::Ok, "global stop of idle axes");
    expect(idle.stream.writes == before + 6, "idle pan tilt zoom focus roll and iris are still stopped");
    expect(idle.motion.haltTracked(idle.head, &now_us) == ccam::Status::Ok, "model and ota stop idle axes");
    expect(idle.stream.writes == before + 10, "four idle axes are stopped again");
}

class NoteOnMove final : public ccam::devices::PtDeviceBase {
public:
    using PtDeviceBase::PtDeviceBase;
    pt::Status power(pt::PowerMode) override { return pt::Status::Param; }
    SessionEndFlag *ends{nullptr};

    ccam::Status move(pt::Axis axis, int8_t speed) override
    {
        if (ends != nullptr && speed != 0) {
            ends->note();
        }
        return PtDeviceBase::move(axis, speed);
    }
};

void test_session_gate_edges()
{
    reset_clock();
    DepthLock lock;
    ScriptStream stream;
    ccam::Transport bus(stream, &now_ms, lock);
    NoteOnMove head(bus);
    MotionAxes motion;
    SessionEndFlag ends;
    head.ends = &ends;
    int halts = 0;
    expect(pass_gate(ends, motion, head, halts, false, true, false).gate == MotionGate::Proceed, "no end admits");
    expect(motion.drive(head, &now_us, pt::Axis::Pan, 10, 0) == ccam::Status::Ok, "velocity during which the session ends");
    expect(motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Hold, "the in-flight command is admitted");
    expect(ends.isPending(), "the end during tx stays pending");
    expect(pass_gate(ends, motion, head, halts, false, true, false).gate == MotionGate::Proceed,
           "the next gate applies the end from tx");
    expect(motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "motion admitted before the end is then stopped");

    reset_clock();
    Rig during;
    SessionEndFlag mid;
    mid.note();
    int mid_halts = 0;
    auto superseded = pass_gate(mid, during.motion, during.head, mid_halts, false, true, true);
    expect(superseded.gate == MotionGate::Superseded, "a new end during the preliminary stop rejects admission");
    expect(mid_halts == 1, "the gate does not loop on that new end");
    expect(mid.isPending(), "the end raised during stop remains pending");

    reset_clock();
    Rig blocked;
    blocked.stream.fail_write_on = 2;
    expect(blocked.motion.haltTracked(blocked.head, &now_us) == ccam::Status::Io, "tilt stop fails");
    expect(blocked.motion.slot(pt::Axis::Pan).mode == MotionAxes::Mode::Idle, "pan stop succeeded");
    expect(blocked.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::StopPending, "tilt remains pending");
    expect(blocked.motion.globalStopOpen(), "the global stop stays open after the flag would be gone");
    const int held = blocked.stream.writes;
    blocked.stream.fail_write_on = 0;
    expect(blocked.motion.drive(blocked.head, &now_us, pt::Axis::Pan, 15, 0) == ccam::Status::Io,
           "a new pan command cannot bypass the open global stop");
    expect(blocked.stream.writes == held, "the bypass sends no velocity");
    g_us += MotionAxes::kStopRetryGapUs;
    blocked.motion.expire(blocked.head, &now_us);
    expect(blocked.motion.slot(pt::Axis::Tilt).mode == MotionAxes::Mode::Idle, "the automatic retry stops tilt");
    expect(!blocked.motion.globalStopOpen(), "the successful retry closes the global stop");
    expect(blocked.motion.drive(blocked.head, &now_us, pt::Axis::Pan, 15, 0) == ccam::Status::Ok,
           "motion is admitted after the global stop completes");

    reset_clock();
    Rig explicit_stop;
    explicit_stop.stream.fail_write_on = 2;
    expect(explicit_stop.motion.haltTracked(explicit_stop.head, &now_us) != ccam::Status::Ok, "stop fails again");
    explicit_stop.stream.fail_write_on = 0;
    expect(explicit_stop.motion.stopAxis(explicit_stop.head, &now_us, pt::Axis::Tilt) == ccam::Status::Ok,
           "an explicit stop is still accepted");
    expect(!explicit_stop.motion.globalStopOpen(), "the explicit stop closes the global duty");

    SessionEndFlag kept;
    kept.note();
    int ota_halts = 0;
    Rig ota;
    expect(pass_gate(kept, ota.motion, ota.head, ota_halts, true, true, false).gate == MotionGate::OtaBusy,
           "ota does not apply the pending end");
    expect(ota_halts == 0, "ota does not stop while the flag must be kept");
    expect(kept.isPending(), "the pending flag survives ota");
}

void test_call_sites()
{
    expect(source_has("src/ph350_ctrl.cpp", "Скорость по умолчанию не меняет Hold/Lease"), "default rate leaves lease alone");
    expect(source_has("src/http_status.cpp", "Сохранение пресета не двигает оси"), "preset save leaves lease alone");
    expect(source_has("src/ota_espota.cpp", "otaHaltMotion"), "ota asks for a halt");
    expect(source_has("src/ph350_ctrl.cpp", "haltTracked"), "model change halts the old object");
    expect(source_has("src/http_status.cpp", "haltMotion"), "home and recall use the four-axis stop");
    expect(source_has("src/http_status.cpp", "admitMotion"), "http motion passes the session gate");
    expect(source_has("src/cli_eth01.cpp", "admit_move"), "cli motion passes the session gate");
    expect(!source_has("src/http_auth.cpp", "DeviceLock"), "auth mutex is not taken while waiting for DeviceLock");
    expect(!source_has("src/motion_axes.hpp", "stopOwned"), "axis state has no session owner");
    expect(source_has("src/http_status.cpp", "stopAxes(Ph350Ctrl::kAimAxes, 2)"), "aim stops pan and tilt");
}

} // namespace

int main()
{
    test_initial_and_hold();
    test_independent_and_resend();
    test_lease_deadline_and_failure();
    test_validation_and_first_failure();
    test_stop_pending_and_retry_gap();
    test_expired_lease_then_new_command();
    test_stop_axes_and_legacy();
    test_session_generation();
    test_preliminary_and_unsupported();
    test_retry_is_bounded();
    test_pre_tx_and_owner_kept();
    test_lease_sum_and_wrap();
    test_session_gate_edges();
    test_call_sites();
    if (g_fails != 0) {
        std::printf("%d failed\n", g_fails);
        return 1;
    }
    std::printf("all passed\n");
    return 0;
}
