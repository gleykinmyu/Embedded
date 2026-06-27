/**
 * Пример 7: McUI overlay — z-order, перекрытие виджетов, modal MsgBox.
 * Приложение: lib/Nextion/examples/example7/app.hpp
 *
 * Сборка: pio run -e example7
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "Debug.h"
#include "server.h"

#include "nex.hpp"
#include "examples/example7/app.hpp"

nex::examples::OverlayZOrderDemoApp app(board.serial2, boardClockMs);

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    NEX_DBG("Nextion example7: overlay z-order demo\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    app.boot();
    app.touch.setAllTouchable(false);

    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        app.update();
    }
}
