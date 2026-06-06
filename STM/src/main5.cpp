/**
 * Пример 5: все конечные виджеты + прогон MCU-API атрибутов.
 * Приложение: lib/Nextion/examples/example5/
 *
 * Сборка: pio run -e example5
 */

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

#include "server.h"
#include "serialConsole.hpp"
#include "examples/example5/app.hpp"

nex::examples::AllComponentsDemoApp app(board.serial2, boardClockMs);

namespace {

nex::examples::AllComponentsDemoApp* g_ex5_app = nullptr;
dbg::SerialConsole g_console(board.serial1);

/** Ждёт Enter на serial1 (debug console); в цикле крутит Nextion `update()`. */
void waitSerial1EnterBeforeComponentDemo() noexcept
{
    NEX_DBG("[ex5] >>> serial1: Enter — следующий виджет... (reboot — сброс MCU)\n");
    g_console.waitEnter([]() noexcept {
        if (g_ex5_app != nullptr)
            g_ex5_app->update();
    });
}

void pollSerial1Console() noexcept
{
    switch (g_console.poll([]() noexcept { app.update(); })) {
    case dbg::ConsoleEvent::None:
    case dbg::ConsoleEvent::Enter:
        break;
    case dbg::ConsoleEvent::Reboot:
        NEX_DBG("[console] reboot\n");
        dbg::softwareReboot();
        break;
    case dbg::ConsoleEvent::Unknown:
        NEX_DBG("[console] unknown: \"%s\" (Enter=next, reboot=reset)\n", g_console.line());
        break;
    }
}

} // namespace

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>, GPIO::ModeAlt::OD);

    board.serial1.open(250000);
    board.serial2.open(250000);

    NEX_DBG("Nextion example5: nexComponents leaves on 4 pages + attribute demo\n");

    board.led.Off();
    uint32_t last_blink_ms = 0;

    app.restartScreen();

    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 3000u)
            app.update();
    }

    app.enableBkcmdAlways();

    {
        const uint32_t t0 = board.GetTick();
        while ((board.GetTick() - t0) < 500u)
            app.update();
    }

    g_ex5_app = &app;
    nex::examples::ex5::beforeEachComponentDemo = waitSerial1EnterBeforeComponentDemo;
    app.runAttributeDemoOnce();

    NEX_DBG("[ex5] main loop (live demos + command queue)\n");

    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
        }
        app.tickLiveDemos(now);
        app.update();
        pollSerial1Console();
    }
}
