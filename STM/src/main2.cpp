/**
 * Пример 2: 2 страницы x 10 кнопок — CompIdMap Discover, затем touch.
 * Приложение: lib/Nextion/examples/example2/app.hpp
 *
 * Сборка: pio run -e example2  (по умолчанию)
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "server.h"
#include "examples/example2/app.hpp"

nex::examples::TenButtonsApp app(board.serial2, boardClockMs);

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    NEX_DBG("Nextion example2: 2 pages x 10 buttons + IdMap discover\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    app.restartScreen();

    // Дать UART обработать `rest` и дождаться готовности панели до Discover.
    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 2000u)
            app.update();
    }

    const uint32_t boot = board.GetTick();
    app.runIdMapDiscover(boot);

    while (app.tickIdMapDiscover(board.GetTick())) {
        if ((board.GetTick() - boot) > 120000u) {
            NEX_DBG("[ex2] Discover timeout\n");
            break;
        }
    }

    if (!app.id_map_poll_ok) {
        NEX_DBG("[ex2] Running without successful IdMap poll (touch may not route)\n");
    } else {
        NEX_DBG("[ex2] Ready for touch (switch pages on panel to test page1)\n");
    }

    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        app.update();
    }
}
