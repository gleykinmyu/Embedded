/**
 * Пример 1: двухстраничное демо (touch, MsgBox, смена страниц).
 * HMI: page0/page1, компоненты с id 1 и 2 — см. examples/example1_two_page_demo/two_page_demo_app.hpp
 *
 * Сборка: pio run -e example1_two_page
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "Debug.h"
#include "server.h"
#include "ff.h"

#include "nex.hpp"
#include "examples/example1_two_page_demo/two_page_demo_app.hpp"

nex::examples::TwoPageTouchDemoApp app(board.serial2);

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(115200);
    board.serial2.open(115200);

    printf("Nextion example1: two-page demo\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    app.restartScreen();
    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        app.update(now);
    }
}
