/**
 * Пример 4: прогон проверок ошибок Nextion (Register / NIS / Session / Gateway / Stream).
 * Приложение: lib/Nextion/examples/example4/app.hpp
 *
 * Сборка: pio run -e example4
 * Монитор: pio device monitor -e example4
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "server.h"
#include "examples/example4/app.hpp"



int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    NEX_DBG("Nextion example4: error path test harness\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    nex::examples::ErrorTestApp app(board.serial2);
    
    app.restartScreen();

    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 500u)
            app.update(board.GetTick());
    }

    app.beginTests();

    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= (app.tests_running ? 250u : 1000u)) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        if (app.tests_running)
            app.tickTests(now);
        else
            app.update(now);
    }
}
