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
#include "fat_file.hpp"
#include "mbrowser.hpp"

nex::AppTiming timing = {boardClockMs, 500u};


server::Application app(board.serial2, nex::Rect(600u, 1024u), timing);

smcp::file::FatVolume sdVolume;
smcp::file::FatFile showFile;
smcp::file::FatDirectory showDir;
MConsole console(showFile);
MBrowser mBrowser(sdVolume, showDir);

namespace {

void tryFlashRoundtrip() noexcept
{
    uint8_t mfr = 0, type = 0, cap = 0;
    if (!board.flash.readJedec(mfr, type, cap)) {
        NEX_DBG("W25Q JEDEC read failed\n");
        return;
    }
    NEX_DBG("W25Q JEDEC: %02X %02X %02X\n", mfr, type, cap);

    if (!board.flash.begin()) {
        NEX_DBG("W25Q begin failed (unexpected JEDEC)\n");
        return;
    }

    constexpr uint32_t kTestAddr = decltype(board.flash)::kSize - 256U;
    const uint8_t pattern[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    uint8_t got[8] = {};

    if (!board.flash.write(kTestAddr, pattern, sizeof(pattern))) {
        NEX_DBG("W25Q write failed\n");
        return;
    }
    if (!board.flash.read(kTestAddr, got, sizeof(got))) {
        NEX_DBG("W25Q read failed\n");
        return;
    }

    const bool ok = (got[0] == pattern[0] && got[1] == pattern[1] && got[2] == pattern[2] &&
                     got[3] == pattern[3] && got[4] == pattern[4] && got[5] == pattern[5] &&
                     got[6] == pattern[6] && got[7] == pattern[7]);
    NEX_DBG("W25Q roundtrip @0x%06lX: %s [%02X %02X %02X %02X]\n",
        static_cast<unsigned long>(kTestAddr), ok ? "OK" : "FAIL", got[0], got[1], got[2],
        got[3]);
}

void tryShowFileRoundtrip() noexcept
{
    if (!sdVolume.mount(board.SD.volumePath())) {
        NEX_DBG("SD mount failed: %d (%s)\n", static_cast<int>(sdVolume.lastResult()),
            board.SD.volumePath());
        return;
    }
    NEX_DBG("SD mounted: %s\n", board.SD.volumePath());

    if (!mBrowser.refresh()) {
        NEX_DBG("MBrowser::refresh failed\n");
        return;
    }
    NEX_DBG("MBrowser: %u files, %u pages\n",
        static_cast<unsigned>(mBrowser.fileCount()),
        static_cast<unsigned>(mBrowser.pageCount()));
    for (std::size_t row = 0; row < mBrowser.pageRows(); ++row) {
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
    NEX_DBG("NEX_TRACE_TX/RX enabled — смотрите serial1 (PA9/PA10)\n");

    if (!board.rtc.begin()) {
        NEX_DBG("RTC begin failed (LSE?)\n");
    } else {
        PHL::DateTime now{};
        if (board.rtc.get(now)) {
            NEX_DBG("RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
                now.year, now.month, now.day, now.hour, now.minute, now.second);
        }
    }

    tryFlashRoundtrip();
    tryShowFileRoundtrip();

    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");

    while (1) {
        board.tick();
        app.update();
    }
}