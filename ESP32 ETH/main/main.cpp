#include "cli_on_stream.hpp"
#include "digital_pin.hpp"
#include "serial.hpp"
#include "stdio.hpp"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstring>

namespace {

extern "C" void onLed(EmbeddedCli* cli, char* args, void* context)
{
    auto* led = static_cast<BIF::IDigitalPin*>(context);
    const char* arg = nullptr;
    if (args != nullptr && args[0] != '\0')
        arg = embeddedCliGetToken(args, 1);

    if (arg == nullptr) {
        embeddedCliPrint(cli, led->Read() ? "led on" : "led off");
        return;
    }
    if (std::strcmp(arg, "on") == 0)
        led->Set();
    else if (std::strcmp(arg, "off") == 0)
        led->Clear();
    else if (std::strcmp(arg, "toggle") == 0)
        led->Toggle();
    else {
        embeddedCliPrint(cli, "led on | off | toggle");
        return;
    }
    embeddedCliPrint(cli, led->Read() ? "led on" : "led off");
}

} // namespace

extern "C" void app_main(void)
{
    /* Консоль IDF уже держит UART0. Снимаем её драйвер и отдаём порт нашему Serial. */
    uart_driver_delete(UART_NUM_0);

    static Usart::Serial<UART_NUM_0> serial;
    serial.InitPins(GPIO_NUM_1, GPIO_NUM_3);
    if (!serial.open(115200)) {
        for (;;)
            vTaskDelay(pdMS_TO_TICKS(1000));
    }

    Usart::bindStdio(&serial);
    setvbuf(stdout, nullptr, _IONBF, 0);

    static DigitalPin<GPIO_NUM_2> led;
    led.Init(BIF::PinMode::Output);

    static Cli::OnStream cli(serial);
    if (!cli.ok() || !cli.add({
            "led",
            "on | off | toggle. Без аргумента печатает состояние.",
            true,
            &led,
            onLed,
        })) {
        printf("cli init failed\n");
        for (;;)
            vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("help, led, Tab\n");

    for (;;) {
        cli.poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
