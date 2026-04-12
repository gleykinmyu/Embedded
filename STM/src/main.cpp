#include <stm32f4xx_hal.h>

extern "C" {
    #include "ff.h"
    }

#include "server.h"
#include "Debug.h"

#include "PUMS/Console.hpp"
#include "PUMS/SDCard.hpp"

static uint32_t pumsMillis() { return HAL_GetTick(); }

static void pumsDelay(uint32_t ms) { HAL_Delay(ms); }

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial2.InitPins(GPIO::PortA::pin<2>, GPIO::PortA::pin<3>);
    board.serial1.open(9600);

    static FATFS fatfs;
    if (f_mount(&fatfs, board.SD.volumePath(), 1) != FR_OK) {
        printf("FATFS mount failed\n");
    }
    printf("FATFS mounted\n");

    static WinchSD         sd(&fatfs, board.SD.volumePath());
    static TouchConsole    MainConsole(board.serial2, pumsMillis, pumsDelay, &sd, 24, 4);
    printf("Console initialized\n");


    MainConsole.begin(115200);
    if (sd.FileList.Refresh() != SDE_OK)
        MainConsole.ShowMsgBox(CMsgBox::MBC_OK, sd.GetErrorText(), 0);

    HAL_Delay(500);

    printf("Start working\n");
    uint32_t last_blink_ms = 0;
    for (;;) {
        MainConsole.Processing(); 

        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();        }  
    }
}