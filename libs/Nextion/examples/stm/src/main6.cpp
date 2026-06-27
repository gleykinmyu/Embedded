/**
 * Пример 6: замер времени ответа панели (тот же HMI, что example5).
 *
 * Сборка: pio run -e example6
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "server.h"
#include "examples/example6/app.hpp"

nex::examples::LatencyBenchApp app(board.serial2, boardClockMs);

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    NEX_DBG("Nextion example6: per-page command latency bench (ex5 HMI)\n");

    board.led.Off();

    app.restartScreen();

    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 3000u)
            app.update();
    }

    setSerial1LogEnabled(false);
    app.enableBkcmdAlways();
    app.waitBkcmdApplied();
    app.runLatencyBenchOnce();
    setSerial1LogEnabled(true);

    app.printBenchSummary();

    NEX_DBG("[ex6] bench done\n");

    while (1) {
    }
}
