/**
 * McUI overlay — z-order, перекрытие виджетов, modal MsgBox.
 * Приложение: ../../libs/Nextion/examples/example7/app.hpp
 *
 * Сборка: pio run
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "Debug.h"
#include "UI/application.hpp"

#include "core/nexDebug.hpp"
#include "core/nexTypes.hpp"
#include "nex.hpp"
#include "board.hpp"

nex::AppTiming timing = {boardClockMs, 500u};


server::Application app(board.serial2, nex::Rect(320u, 240u), timing);

MConsole console;

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    setSerial1LogEnabled(true);
    board.setLedAlive(true);

    NEX_DBG("PUMS Console boot: log=serial1 Nextion=serial2 250000\n");
    NEX_DBG("NEX_TRACE_TX/RX enabled — смотрите serial1 (PA9/PA10)\n");

    app.boot();
    NEX_DBG("Application::boot() done, entering main loop\n");

    while (1) {
        board.tick();
        app.update();
    }
}