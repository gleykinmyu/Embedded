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


server::Application app(board.serial2, nex::Rect(320u, 240u), timing);

smcp::file::FatVolume sdVolume;
smcp::file::FatFile showFile;
smcp::file::FatDirectory showDir;
MConsole console(showFile);
MBrowser mBrowser(sdVolume, showDir);

namespace {

constexpr char kShowPath[] = "0:/SHOW";

void tryShowFileRoundtrip() noexcept
{
    if (!sdVolume.mount(board.SD.volumePath())) {
        NEX_DBG("SD mount failed: %d (%s)\n", static_cast<int>(sdVolume.lastResult()),
            board.SD.volumePath());
        return;
    }
    NEX_DBG("SD mounted: %s\n", board.SD.volumePath());

    if (!console.saveShow(kShowPath)) {
        NEX_DBG("saveShow failed: %d\n", static_cast<int>(showFile.lastResult()));
        return;
    }
    NEX_DBG("saveShow OK: %s exists=%d\n", kShowPath,
        sdVolume.exists(kShowPath) ? 1 : 0);

    if (!console.loadShow(kShowPath)) {
        NEX_DBG("loadShow failed: %d\n", static_cast<int>(showFile.lastResult()));
        return;
    }
    NEX_DBG("loadShow OK, show=\"%s\"\n", console.showName());

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

    board.serial1.open(250000);
    board.serial2.open(250000);

    setSerial1LogEnabled(true);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    board.setLedAlive(true);

    NEX_DBG("PUMS Console boot: log=serial1 Nextion=serial2 250000\n");
    NEX_DBG("NEX_TRACE_TX/RX enabled — смотрите serial1 (PA9/PA10)\n");

    tryShowFileRoundtrip();

    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");

    while (1) {
        board.tick();
        app.update();
    }
}