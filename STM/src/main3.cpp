/**
 * Пример 3: 10 кнопок с известными comp_id 1…10 (Compiled, без Discover).
 * Приложение: lib/Nextion/examples/example3/app.hpp
 *
 * Сборка: pio run -e example3
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "server.h"
#include "examples/example3/app.hpp"

nex::examples::TenButtonsCompiledApp app(board.serial2);

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    printf("Nextion example3: ten buttons, compiled ids 1..10\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    app.restartScreen();
    app.touch.touchSwitch(true);

    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 500u)
            app.update(board.GetTick());
    }

    printf("[10btn-id] Ready for touch\n");

    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        app.update(now);
    }
}
