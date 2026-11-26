#include "device_guard.hpp"
#include "devices/pt_device.hpp"
#include "transport/transport.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool cond, const char *name)
{
    if (!cond) {
        std::printf("FAIL %s\n", name);
        ++g_fails;
    } else {
        std::printf("PASS %s\n", name);
    }
}

uint32_t g_ms = 0;

uint32_t now_ms()
{
    return g_ms;
}

class DepthLock final : public BIF::ILockable {
public:
    int depth{0};
    int max_depth{0};

protected:
    void lock() override
    {
        ++depth;
        if (depth > max_depth) {
            max_depth = depth;
        }
    }
    void unlock() override { --depth; }
};

class TestHead final : public ccam::devices::PtDeviceBase {
public:
    using PtDeviceBase::PtDeviceBase;
    pt::Status power(pt::PowerMode) override { return pt::Status::Param; }
};

class ScriptStream final : public BIF::IByteStream {
public:
    DepthLock *lock{nullptr};
    int fail_write_after{0};
    int fail_flush_on{0};
    int partial_limit{0};
    int writes{0};
    int purges{0};
    int flushes{0};
    int reads{0};
    std::string events;
    int depth_at_io{0};
    bool depth_ok{true};

    size_t write(const uint8_t *, size_t size) override
    {
        note('W');
        ++writes;
        if (partial_limit > 0 && writes == 1) {
            return size < static_cast<size_t>(partial_limit) ? size : static_cast<size_t>(partial_limit);
        }
        if (fail_write_after > 0 && writes >= fail_write_after) {
            g_ms += 200;
            return 0;
        }
        if (partial_limit > 0 && writes > 1) {
            g_ms += 200;
            return 0;
        }
        return size;
    }
    size_t read(uint8_t *buffer, size_t maxSize) override
    {
        note('R');
        ++reads;
        if (buffer == nullptr || maxSize == 0) {
            return 0;
        }
        buffer[0] = static_cast<uint8_t>('\r');
        return 1;
    }
    size_t available() const override { return 0; }
    size_t availableForWrite() const override { return 64; }
    void purge() override
    {
        note('P');
        ++purges;
    }
    void purgeOutput() override {}
    bool flushTx() override
    {
        note('F');
        ++flushes;
        return fail_flush_on == 0 || flushes < fail_flush_on;
    }
    void flush() override { (void)flushTx(); }
    bool open(uint32_t) override { return true; }
    void close() override {}
    bool isOpen() override { return true; }
    Status getStatus() override { return Status::OK; }
    void clearErrors() override {}

private:
    void note(char mark)
    {
        events.push_back(mark);
        if (lock != nullptr && lock->depth != 1) {
            depth_ok = false;
            depth_at_io = lock->depth;
        }
    }
};

void test_guard()
{
    expect(device_guard(3, 3, false) == DeviceGuard::Ok, "guard same epoch");
    expect(device_guard(3, 4, false) == DeviceGuard::EpochChanged, "guard epoch change");
    expect(device_guard(3, 4, true) == DeviceGuard::EpochChanged, "guard epoch before ota");
    expect(device_guard(3, 3, true) == DeviceGuard::OtaBusy, "guard ota same epoch");
}

void test_stop_all()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    ccam::Transport bus(stream, &now_ms, lock);
    TestHead head(bus);
    const ccam::Status st = head.stop();
    expect(st == ccam::Status::Ok, "stop all ok");
    expect(stream.purges == 6, "stop six purges");
    expect(stream.flushes == 12, "stop six idle waits and six tx done");
    expect(stream.reads == 0, "stop does not read");
}

void test_stop_continues()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    stream.fail_write_after = 1;
    ccam::Transport bus(stream, &now_ms, lock);
    TestHead head(bus);
    const int purges_before = stream.purges;
    const ccam::Status st = head.stop();
    expect(st == ccam::Status::Io, "stop first error");
    expect(stream.purges - purges_before == 6, "stop tries six after error");
    expect(stream.flushes == 6, "failed writes skip tx done");
    expect(stream.reads == 0, "stop error does not read");
}

void test_stop_keeps_first()
{
    g_ms = 1000;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    stream.fail_write_after = 3;
    ccam::Transport bus(stream, &now_ms, lock);
    TestHead head(bus);
    const ccam::Status st = head.stop();
    expect(st == ccam::Status::Io, "stop keeps first io");
    expect(stream.purges == 6, "later axes still attempted");
    expect(stream.flushes == 8, "two axes completed tx, four only waited idle");
}

void test_transact_no_mix()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    ccam::Transport bus(stream, &now_ms, lock);
    uint8_t rx[8] = {};
    size_t n = 0;
    const uint8_t tx[] = {'#', 'O', '\r'};
    expect(bus.transact(tx, sizeof(tx), rx, sizeof(rx), &n, 350) == ccam::Status::Ok, "transact 1");
    expect(bus.transact(tx, sizeof(tx), rx, sizeof(rx), &n, 350) == ccam::Status::Ok, "transact 2");
    expect(stream.events == "FPWFRFPWFR", "frames do not overlap");
    expect(stream.depth_ok, "lock held for write and read");
    expect(lock.max_depth == 1, "transport lock not nested");
    expect(lock.depth == 0, "transport lock released");
}

void test_write_timeout_skips_rx()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    stream.fail_write_after = 1;
    ccam::Transport bus(stream, &now_ms, lock);
    uint8_t rx[8] = {};
    size_t n = 99;
    const uint8_t tx[] = {'#', 'O', '\r'};
    const ccam::Status st = bus.transact(tx, sizeof(tx), rx, sizeof(rx), &n, 350);
    expect(st == ccam::Status::Io, "write timeout is io");
    expect(stream.events == "FPW", "no tx-done and no rx after write timeout");
    expect(n == 0, "rx length cleared");
}

void test_tx_done_not_ok()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    stream.fail_flush_on = 2;
    ccam::Transport bus(stream, &now_ms, lock);
    const uint8_t tx[] = {'#', 'P', '5', '0', '\r'};
    expect(bus.send(tx, sizeof(tx)) == ccam::Status::Io, "tx done error is not ok");
    expect(stream.events == "FPWF", "tx done waited after the bytes were queued");
    expect(stream.reads == 0, "send does not read");
}

void test_transact_skips_rx_after_tx_done()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    stream.fail_flush_on = 2;
    ccam::Transport bus(stream, &now_ms, lock);
    uint8_t rx[8] = {};
    size_t n = 99;
    const uint8_t tx[] = {'#', 'O', '\r'};
    const ccam::Status st = bus.transact(tx, sizeof(tx), rx, sizeof(rx), &n, 350);
    expect(st == ccam::Status::Io, "transact tx done error");
    expect(stream.reads == 0, "transact does not start rx after tx error");
    expect(n == 0, "rx length stays zero");
    expect(stream.events == "FPWF", "no read after failed tx done");
}

void test_partial_write()
{
    g_ms = 0;
    DepthLock lock;
    ScriptStream stream;
    stream.lock = &lock;
    stream.partial_limit = 1;
    ccam::Transport bus(stream, &now_ms, lock);
    uint8_t rx[8] = {};
    size_t n = 99;
    const uint8_t tx[] = {'#', 'O', '\r'};
    const ccam::Status st = bus.transact(tx, sizeof(tx), rx, sizeof(rx), &n, 350);
    expect(st == ccam::Status::Io, "partial write is not ok");
    expect(stream.writes >= 2, "partial write is retried");
    expect(stream.reads == 0, "partial write does not start rx");
    expect(stream.events == "FPWW", "tx done not claimed for a short frame");
}

} // namespace

int main()
{
    test_guard();
    test_stop_all();
    test_stop_continues();
    test_stop_keeps_first();
    test_transact_no_mix();
    test_write_timeout_skips_rx();
    test_tx_done_not_ok();
    test_transact_skips_rx_after_tx_done();
    test_partial_write();
    if (g_fails != 0) {
        std::printf("%d failed\n", g_fails);
        return 1;
    }
    std::printf("all passed\n");
    return 0;
}
