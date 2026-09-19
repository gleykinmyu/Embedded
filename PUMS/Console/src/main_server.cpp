/**
 * PUMS Server — сегмент SMCP + Nextion 4.3" (Server 1.HMI).
 * UI: page0.cnsl (UART CLI) + StatusBar снизу.
 *
 * Сборка: pio run -e server
 */

#include <cstdio>
#include <cstring>

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "Debug.h"
#include "ServerUI/application.hpp"
#include "board.hpp"
#include "core/crash_dump.hpp"
#include "core/memstat.hpp"
#include "core/nexDebug.hpp"
#include "model/mserver.hpp"
#include "nex.hpp"
#include "smcp/mock_can.hpp"
#include "smcp/transport/can_link.hpp"

smcp::MockCan canServer;
smcp::CanLink linkServer(canServer, smcp::msg::kServerIdMin);
MServer mServer(linkServer, boardClockMs);

nex::AppTiming timing = {boardClockMs, 500u};
segment::Application app(board.serial2, nex::Rect(480u, 272u), timing);

namespace {

constexpr uint32_t kWatchdogTimeoutMs = 5000u;

char g_stdoutBuf[256];

void sendNexRaw(const char* cmd) noexcept
{
    const auto n = static_cast<size_t>(std::strlen(cmd));
    (void)board.serial2.write(reinterpret_cast<const uint8_t*>(cmd), n);
    static const uint8_t kTerm[3] = {0xFFu, 0xFFu, 0xFFu};
    (void)board.serial2.write(kTerm, sizeof(kTerm));
    board.serial2.flush();
}

void waitMs(uint32_t ms) noexcept
{
    const uint32_t t0 = board.GetTick();
    while ((board.GetTick() - t0) < ms) {
        board.watchdog.kick();
    }
}

[[nodiscard]] bool probeNexBaud(uint32_t baud) noexcept
{
    board.serial2.close();
    board.watchdog.kick();
    if (!board.serial2.open(baud)) {
        NEX_DBG("nex probe: open %lu failed\n", static_cast<unsigned long>(baud));
        return false;
    }
    board.serial2.purge();
    board.serial2.clearErrors();

    sendNexRaw("sendme");
    waitMs(400u);

    const size_t n = board.serial2.available();
    NEX_DBG("nex probe baud=%lu rx=%u st=%s\n",
        static_cast<unsigned long>(baud),
        static_cast<unsigned>(n),
        BIF::cstr(board.serial2.getStatus()));
    if (n == 0u) {
        return false;
    }

    uint8_t buf[32]{};
    const size_t got = board.serial2.read(buf, n < sizeof(buf) ? n : sizeof(buf));
    NEX_DBG("nex rx:");
    for (size_t i = 0u; i < got; ++i) {
        NEX_DBG(" %02X", static_cast<unsigned>(buf[i]));
    }
    NEX_DBG("\n");
    return true;
}

void probeNexLink() noexcept
{
    static const GPIO::ModeAlt kModes[] = {GPIO::ModeAlt::PP, GPIO::ModeAlt::OD};
    static const uint32_t kBauds[] = {250000u, 9600u, 115200u};

    for (const GPIO::ModeAlt mode : kModes) {
        NEX_DBG("nex probe pins %s (PA2 TX / PA3 RX)\n",
            mode == GPIO::ModeAlt::PP ? "PP" : "OD");
        board.serial2.close();
        board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, mode);
        for (const uint32_t baud : kBauds) {
            if (probeNexBaud(baud)) {
                NEX_DBG("nex probe: locked baud=%lu\n", static_cast<unsigned long>(baud));
                sendNexRaw("bkcmd=0");
                waitMs(50u);
                board.serial2.purge();
                return;
            }
        }
    }

    NEX_DBG("nex probe: silence — нет ответа 4.3\" (питание, .tft, TX/RX?)\n");
    board.serial2.close();
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::PP);
    (void)board.serial2.open(250000);
}

} // namespace

int main(void)
{
    memstat::boot();

    const bool resetByWatchdog = PHL::WatchDog::causedReset();

    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial1.open(250000);

    setSerial1LogEnabled(true);
    std::setvbuf(stdout, g_stdoutBuf, _IOLBF, sizeof(g_stdoutBuf));
    board.setLedAlive(true);

    NEX_DBG("PUMS Server boot: log=serial1, Nextion=serial2 (probe baud/pins)\n");
    if (resetByWatchdog) {
        NEX_DBG("*** Reset caused by IWDG watchdog ***\n");
    }
    {
        CrashDump::Record crash{};
        if (CrashDump::take(crash)) {
            NEX_DBG("*** Crash %s CFSR=%08X HFSR=%08X MMFAR=%08X BFAR=%08X\n"
                    "    R0=%08X R1=%08X R2=%08X R3=%08X\n"
                    "    R12=%08X LR=%08X PC=%08X xPSR=%08X MSP=%08X PSP=%08X ***\n",
                CrashDump::kindName(crash.kind),
                crash.cfsr, crash.hfsr, crash.mmfar, crash.bfar,
                crash.r0, crash.r1, crash.r2, crash.r3,
                crash.r12, crash.lr, crash.pc, crash.xpsr,
                crash.msp, crash.psp);
        }
    }
    PHL::WatchDog::clearResetFlags();

    /* LSE может стартовать секунды — до IWDG. */
    bool rtcFromBuild = false;
    PHL::DateTime rtcNow{};
    if (!PHL::initRtc(board.rtc, &rtcNow, &rtcFromBuild)) {
        NEX_DBG("RTC begin failed (LSE? VBAT?)\n");
    } else {
        NEX_DBG("RTC %s: %04u-%02u-%02u %02u:%02u:%02u\n",
            rtcFromBuild ? "set from build" : "restored",
            rtcNow.year, rtcNow.month, rtcNow.day,
            rtcNow.hour, rtcNow.minute, rtcNow.second);
    }

    if (!board.watchdog.begin(kWatchdogTimeoutMs)) {
        NEX_DBG("IWDG begin failed (timeout=%u ms)\n", kWatchdogTimeoutMs);
    } else {
        NEX_DBG("IWDG started: %u ms (PR=%u RLR=%u)\n",
            board.watchdog.timeoutMs(),
            board.watchdog.prescaler(),
            board.watchdog.reload());
    }

    board.watchdog.kick();

    (void)canServer.open(1'000'000);

    board.watchdog.kick();
    probeNexLink();
    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");
    NEX_DBG("SMCP server id=0x%02X mechs=%u sessions=%u\n",
        static_cast<unsigned>(mServer.id()),
        static_cast<unsigned>(mServer.mechCapacity()),
        static_cast<unsigned>(mServer.sessionCapacity()));

    unsigned testTick = 0u;
    while (1) {
        if (board.tick()) {
            ++testTick;
            app.writeTest(testTick);
        }
        mServer.update();
        app.update();
    }
}
