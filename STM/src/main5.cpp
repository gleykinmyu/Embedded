/**
 * Пример 5: все конечные виджеты + прогон MCU-API атрибутов.
 * Приложение: lib/Nextion/examples/example5/
 *
 * Сборка: pio run -e example5
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "server.h"
#include "examples/example5/app.hpp"

nex::examples::AllComponentsDemoApp app(board.serial2);

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    NEX_DBG("Nextion example5: all leaf components + attribute demo\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    app.restartScreen();

    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 3000u)
            app.update(board.GetTick());
    }

    app.runAttributeDemoOnce();

    NEX_DBG("[ex5] main loop (drain command queue)\n");

    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        app.update(now);
    }
}
