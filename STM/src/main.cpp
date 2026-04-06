#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include "server.h"

CServerBoard board;

namespace {
constexpr char kUart1TestLine[] = "UART1 test\r\n";
}

int main(void)
{
    board.uart1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.uart1.open(9600);
    board.led.Off();
    while (1) {
        board.led.Toggle();
        board.uart1.write((uint8_t*)kUart1TestLine, sizeof(kUart1TestLine) - 1U);
        //board.uart1.flush();
        board.Delay(1000);
    }
}