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
#include "mbrowser.hpp"

nex::AppTiming timing = {boardClockMs, 500u};


server::Application app(board.serial2, nex::Rect(600u, 1024u), timing);

smcp::file::FatVolume sdVolume;
smcp::file::FatFile showFile;
smcp::file::FatDirectory showDir;
MConsole console;
MBrowser mBrowser(sdVolume, showDir, showFile);

namespace {

/** Таймаут IWDG на время boot (SD) и работы; kick — в board.tick(). */
constexpr uint32_t kWatchdogTimeoutMs = 5000u;

void tryShowFileRoundtrip() noexcept
{
    board.watchdog.kick();

    if (!sdVolume.mount(board.SD.volumePath())) {
        NEX_DBG("SD mount failed: %d (%s)\n", static_cast<int>(sdVolume.lastResult()),
            board.SD.volumePath());
        return;
    }
    NEX_DBG("SD mounted: %s\n", board.SD.volumePath());

    board.watchdog.kick();

    if (!mBrowser.refresh()) {
        NEX_DBG("MBrowser::refresh failed\n");
        return;
    }
    NEX_DBG("MBrowser: %u files, %u pages\n",
        static_cast<unsigned>(mBrowser.fileCount()),
        static_cast<unsigned>(mBrowser.pageCount()));
    for (std::size_t row = 0; row < mBrowser.pageRows(); ++row) {
        board.watchdog.kick();
        const MBrowser::Entry* e = mBrowser.entryAt(row);
        if (e == nullptr) {
            break;
        }
        NEX_DBG("  [%u] %s  %lu\n", static_cast<unsigned>(row), e->name,
            static_cast<unsigned long>(e->size));
    }
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
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    board.setLedAlive(true);

    NEX_DBG("PUMS Console boot: log=serial1 Nextion=serial2 250000 flashSpi=SPI3 8MHz\n");
    if (resetByWatchdog) {
        NEX_DBG("*** Reset caused by IWDG watchdog ***\n");
    }
    {
        CrashDump::Record crash{};
        if (CrashDump::take(crash)) {
            NEX_DBG("*** Crash %s CFSR=%08lX HFSR=%08lX MMFAR=%08lX BFAR=%08lX\n"
                    "    R0=%08lX R1=%08lX R2=%08lX R3=%08lX\n"
                    "    R12=%08lX LR=%08lX PC=%08lX xPSR=%08lX MSP=%08lX PSP=%08lX ***\n",
                CrashDump::kindName(crash.kind),
                static_cast<unsigned long>(crash.cfsr),
                static_cast<unsigned long>(crash.hfsr),
                static_cast<unsigned long>(crash.mmfar),
                static_cast<unsigned long>(crash.bfar),
                static_cast<unsigned long>(crash.r0),
                static_cast<unsigned long>(crash.r1),
                static_cast<unsigned long>(crash.r2),
                static_cast<unsigned long>(crash.r3),
                static_cast<unsigned long>(crash.r12),
                static_cast<unsigned long>(crash.lr),
                static_cast<unsigned long>(crash.pc),
                static_cast<unsigned long>(crash.xpsr),
                static_cast<unsigned long>(crash.msp),
                static_cast<unsigned long>(crash.psp));
        }
    }
    PHL::WatchDog::clearResetFlags();

    if (!board.watchdog.begin(kWatchdogTimeoutMs)) {
        NEX_DBG("IWDG begin failed (timeout=%lu ms)\n",
            static_cast<unsigned long>(kWatchdogTimeoutMs));
    } else {
        NEX_DBG("IWDG started: %lu ms (PR=%lu RLR=%lu)\n",
            static_cast<unsigned long>(board.watchdog.timeoutMs()),
            static_cast<unsigned long>(board.watchdog.prescaler()),
            static_cast<unsigned long>(board.watchdog.reload()));
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

    tryShowFileRoundtrip();

    board.watchdog.kick();
    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");

    while (1) {
        board.tick();
        app.update();
    }
}
