/**
 * PUMS Console — пульт, 10" Nextion, SMCP по CAN1 (PD0 RX / PD1 TX).
 *
 * Сборка: pio run -e console
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
#include "smcp/transport/can_link.hpp"
#include "w25q_show_file.hpp"

smcp::file::FatVolume sdVolume(board.SD.volumePath());
smcp::file::FatFile showFile;
smcp::file::FatDirectory showDir;
smcp::file::W25qShowFile flashShow(board.flash);

smcp::CanLink linkConsole(board.can, 1u);
server::UiConsole console(sdVolume, showDir, showFile, flashShow, linkConsole, boardClockMs);

nex::AppTiming timing = {boardClockMs, 500u};
server::Application app(board.serial2, nex::Rect(600u, 1024u), timing);

namespace {

/** Таймаут IWDG на время boot (SD) и работы; kick — в board.tick(). */
constexpr uint32_t kWatchdogTimeoutMs = 6000u;

/** Буфер stdout в BSS — setvbuf без malloc (newlib иначе выделяет кучу). */
char g_stdoutBuf[256];

#if defined(CAN_DEBUG)

[[nodiscard]] const char* canLecCstr(uint32_t lec) noexcept
{
    switch (lec) {
    case 0u: return "none";
    case 1u: return "stuff";
    case 2u: return "form";
    case 3u: return "ack";
    case 4u: return "bitR";
    case 5u: return "bitD";
    case 6u: return "crc";
    default: return "sw";
    }
}

[[nodiscard]] const char* ilinkCstr(smcp::ILink::Status s) noexcept
{
    switch (s) {
    case smcp::ILink::Status::OK: return "OK";
    case smcp::ILink::Status::EncodeFailed: return "Encode";
    case smcp::ILink::Status::DecodeFailed: return "Decode";
    case smcp::ILink::Status::SendFailed: return "SendFail";
    case smcp::ILink::Status::Closed: return "Closed";
    }
    return "?";
}

void logCanDebug() noexcept
{
    auto& hw = board.can.can();
    const uint32_t esr = hw.esr.read_raw();
    const uint32_t lec = (esr >> 4) & 7u;
    const unsigned tec = (esr >> 16) & 0xFFu;
    const unsigned rec = (esr >> 24) & 0xFFu;
    unsigned tme = 0u;
    if (hw.tsr.any(::CAN::TSR::TME0)) {
        ++tme;
    }
    if (hw.tsr.any(::CAN::TSR::TME1)) {
        ++tme;
    }
    if (hw.tsr.any(::CAN::TSR::TME2)) {
        ++tme;
    }

    std::printf(
        "[CAN] hw=%s open=%u lec=%s tec=%u rec=%u %s%s%s tme=%u/3 fif0=%u rxq=%u txfree=%u "
        "ph=%s node=%s link=%s up=%u\n",
        BIF::CAN::cstr(board.can.getStatus()),
        board.can.isOpen() ? 1u : 0u,
        canLecCstr(lec),
        tec,
        rec,
        (esr & CAN_ESR_EWGF) != 0u ? "EWG " : "",
        (esr & CAN_ESR_EPVF) != 0u ? "EPV " : "",
        (esr & CAN_ESR_BOFF) != 0u ? "BOFF " : "",
        tme,
        static_cast<unsigned>(hw.rx[0].pending()),
        static_cast<unsigned>(board.can.available()),
        static_cast<unsigned>(board.can.availableForWrite()),
        smcp::IConsole::cstr(console.phase()),
        smcp::Node::cstr(console.getStatus()),
        ilinkCstr(linkConsole.getStatus()),
        console.linkUp() ? 1u : 0u);
}

#endif

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

    if (!board.initCan()) {
        NEX_DBG("CAN1 open failed: PD0 RX / PD1 TX %lu bps\n",
            static_cast<unsigned long>(CBoard::kCanBitrate));
    } else {
        NEX_DBG("CAN1 OK: PD0 RX / PD1 TX %lu bps\n",
            static_cast<unsigned long>(CBoard::kCanBitrate));
    }
#if defined(CAN_DEBUG)
    std::printf("[CAN] init open=%u btr=0x%08lX %lu bps PD0 RX / PD1 TX\n",
        board.can.isOpen() ? 1u : 0u,
        static_cast<unsigned long>(board.can.can().btr.read_raw()),
        static_cast<unsigned long>(CBoard::kCanBitrate));
    logCanDebug();
#endif

    console.begin(smcp::msg::kServerIdMin);

    if (console.fio.restore()) {
        NEX_DBG("Restored show from W25Q: '%s'\n", console.show.name());
    } else {
        NEX_DBG("No valid show mirror in W25Q (%s)\n",
            smcp::file::FIOManager::cstr(console.fio.status()));
    }

    board.watchdog.kick();
    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");
    NEX_DBG("SMCP console id=0x%02X, server id=0x%02X\n",
        static_cast<unsigned>(linkConsole.nodeId()),
        static_cast<unsigned>(smcp::msg::kServerIdMin));

    while (1) {
        if (board.tick()) {
#if defined(CAN_DEBUG)
            logCanDebug();
#endif
        }
        console.update();
        app.update();
    }
}
