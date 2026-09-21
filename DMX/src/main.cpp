/**
 * DMX-тестер: STM32F407 + Nextion 10" 1024×600 landscape.
 * Плата как PUMS Console: log=USART1 PA9/PA10, Nextion=USART2 PA2/PA3.
 * DMX RS485: USART3 PB10/PB11, DE=PB1.
 *
 * Сборка: pio run -e tester
 */

#include <cstdio>

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "Debug.h"
#include "UI/application.hpp"
#include "board.hpp"
#include "core/crash_dump.hpp"
#include "core/memstat.hpp"
#include "core/nexDebug.hpp"
#include "model/tester.hpp"
#include "phl/rtc.hpp"
#include "phl/uart.hpp"
#include "rs485.hpp"

namespace {

constexpr uint32_t kWatchdogTimeoutMs = 6000u;
constexpr uint32_t kDmxTxPeriodMs = 25u;
constexpr uint32_t kDmxRxPeriodMs = 100u;

char g_stdoutBuf[256];

dmx::Rs485Port dmxPort(board.dmx, board.dmxTx, GPIO::AF::AF7, delayUs, &board.dmxDe);
ui::Tester tester(dmxPort);

nex::AppTiming timing{boardClockMs, 500u};
ui::Application app(board.serial2, timing);

} // namespace

int main()
{
    memstat::boot();

    const bool resetByWatchdog = PHL::WatchDog::causedReset();

    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);
    board.flashSpi.InitPins(GPIO::PortB::pin<3>, GPIO::PortB::pin<4>, GPIO::PortB::pin<5>);

    board.serial1.open(250000);
    board.serial2.open(ui::Application::kLinkBaudBoot);
    board.flashSpi.open(8'000'000);

    setSerial1LogEnabled(true);
    std::setvbuf(stdout, g_stdoutBuf, _IOLBF, sizeof(g_stdoutBuf));
    board.setLedAlive(true);

    NEX_DBG("DMX tester boot: log=serial1 Nextion=serial2 %u->%u flashSpi=SPI3 8MHz\n",
        static_cast<unsigned>(ui::Application::kLinkBaudBoot),
        static_cast<unsigned>(ui::Application::kLinkBaudFast));
    if (!board.flash.begin())
        NEX_DBG("W25Q begin failed\n");
    else
        NEX_DBG("W25Q OK\n");
    if (resetByWatchdog)
        NEX_DBG("*** Reset caused by IWDG watchdog ***\n");

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

    if (!board.initDmxPins()) {
        NEX_DBG("DMX USART3 8N2 setup failed (PB10/PB11 DE=PB1)\n");
    } else if (!dmxPort.open()) {
        NEX_DBG("DMX port open failed\n");
    } else {
        dmxPort.setDirection(dmx::Direction::Receive);
        NEX_DBG("DMX RS485 RX: USART3 PB10 TX / PB11 RX DE=PB1 250000 8N2\n");
    }

    board.watchdog.kick();
    app.view.bind(&tester);
    app.graph.bind(&tester);
    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");

    uint32_t lastTxMs = boardClockMs();
    uint32_t lastRxMs = lastTxMs;
    uint32_t lastTraceMs = lastTxMs;

    for (;;) {
        board.tick();
        tester.poll();
        app.update();
        app.applyFastBaudIfNeeded();

        const uint32_t now = boardClockMs();
        if (dmxPort.isOpen() && dmxPort.direction() == dmx::Direction::Receive
            && (now - lastRxMs) >= kDmxRxPeriodMs) {
            lastRxMs = now;
            tester.tick();
        }
        if (dmxPort.isOpen() && dmxPort.direction() == dmx::Direction::Transmit
            && (now - lastTxMs) >= kDmxTxPeriodMs) {
            lastTxMs = now;
            tester.sendLive();
        }
        if (app.graph.isVisible()) {
            if ((now - lastTraceMs) >= ui::kTracePeriodMs) {
                lastTraceMs = now;
                tester.sampleTrace();
            }
        } else {
            lastTraceMs = now;
        }
        app.refreshUi();
    }
}
