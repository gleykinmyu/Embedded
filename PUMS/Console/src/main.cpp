/**
 * McUI overlay — z-order, перекрытие виджетов, modal MsgBox.
 * Приложение: ../../libs/Nextion/examples/example7/app.hpp
 *
 * Сборка: pio run
 */

#include <cstdio>

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "Debug.h"
#include "UI/application.hpp"

#include "core/nexDebug.hpp"
#include "core/nexTypes.hpp"
#include "nex.hpp"
#include "board.hpp"
#include "core/memstat.hpp"
#include "core/crash_dump.hpp"
#include "fat_file.hpp"
#include "model/mbrowser.hpp"
#include "model/mconsole.hpp"
#include "model/mserver.hpp"
#include "w25q_show_file.hpp"


smcp::file::FatVolume sdVolume(board.SD.volumePath());
smcp::file::FatFile showFile;
smcp::file::FatDirectory showDir;
smcp::file::W25qShowFile flashShow(board.flash);
MBrowser mBrowser(sdVolume, showDir, showFile);
MConsole console(mBrowser);
MServer mServer(smcp::msg::kServerIdMin);

nex::AppTiming timing = {boardClockMs, 500u};
server::Application app(board.serial2, nex::Rect(600u, 1024u), timing);

namespace {

/** Таймаут IWDG на время boot (SD) и работы; kick — в board.tick(). */
constexpr uint32_t kWatchdogTimeoutMs = 5000u;

/** Буфер stdout в BSS — setvbuf без malloc (newlib иначе выделяет кучу). */
char g_stdoutBuf[256];

/** console.tx → server.rx → poll → server.tx → console.rx → poll → UI */
void pumpSmcpLoopback() noexcept
{
    smcp::msg::Packet pkt;
    while (console.tx().pop(pkt)) {
        (void)mServer.rx().push(pkt);
    }
    mServer.poll();
    while (mServer.tx().pop(pkt)) {
        (void)console.rx().push(pkt);
    }
    console.poll();
    app.applyTelemetryUi();
}

} // namespace

int main(void)
{
    memstat::boot();

    /* Флаг RCC до clear — иначе потеряем причину сброса. */
    const bool resetByWatchdog = PHL::WatchDog::causedReset();

    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);
    // W25Q16 на SPI3: SCK=PB3, MISO=PB4, MOSI=PB5; CS=PA15 — у board.flash
    board.flashSpi.InitPins(GPIO::PortB::pin<3>, GPIO::PortB::pin<4>, GPIO::PortB::pin<5>);

    board.serial1.open(250000);
    board.serial2.open(250000);
    board.flashSpi.open(8'000'000);

    setSerial1LogEnabled(true);
    /* _IOLBF: сброс на '\n' — удобно для NEX_DBG; буфер свой, не heap. */
    std::setvbuf(stdout, g_stdoutBuf, _IOLBF, sizeof(g_stdoutBuf));
    board.setLedAlive(true);

    NEX_DBG("PUMS Console boot: log=serial1 Nextion=serial2 250000 flashSpi=SPI3 8MHz\n");
    if (!board.flash.begin()) {
        NEX_DBG("W25Q begin failed\n");
    } else {
        NEX_DBG("W25Q OK, show mirror sector @ 0x%06X\n",
            smcp::file::W25qShowFile::kSectorAddr);
    }
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

    if (!board.watchdog.begin(kWatchdogTimeoutMs)) {
        NEX_DBG("IWDG begin failed (timeout=%u ms)\n", kWatchdogTimeoutMs);
    } else {
        NEX_DBG("IWDG started: %u ms (PR=%u RLR=%u)\n",
            board.watchdog.timeoutMs(),
            board.watchdog.prescaler(),
            board.watchdog.reload());
    }

    board.watchdog.kick();

    if (!board.rtc.begin()) {
        NEX_DBG("RTC begin failed (LSE?)\n");
    } else {
        PHL::DateTime now{};
        if (board.rtc.get(now)) {
            NEX_DBG("RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
                now.year, now.month, now.day, now.hour, now.minute, now.second);
        }
    }

    board.watchdog.kick();
    console.setMirror(&flashShow);
    console.setFlushHook(pumpSmcpLoopback);
    if (console.restoreMirror()) {
        NEX_DBG("Restored show from W25Q: '%s'\n", console.showName());
    } else {
        NEX_DBG("No valid show mirror in W25Q (%s)\n",
            MConsole::statusText(console.getStatus()));
    }

    board.watchdog.kick();
    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");
    NEX_DBG("SMCP loopback: server id=0x%02X\n", static_cast<unsigned>(mServer.id()));

    while (1) {
        board.tick();
        pumpSmcpLoopback();
        app.update();
    }
}
