#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include "Debug.h"
#include "server.h"
#include "ff.h"

CServerBoard board;

namespace {
static FATFS s_fatfs;

/** Монтирование тома и запись тестового файла через FatFs (том «0:» задаётся при link в IDisk). */
void fatfs_demo()
{
    /* Для f_mount ChaN указывает префикс логического диска «0:», не «0:/». */
    FRESULT fr = f_mount(&s_fatfs, board.SD.volumePath(), 1);
    if (fr != FR_OK)
        return;
    printf("Mount SD card successfully\n\r");

    FIL  fil{};
    UINT written = 0;
    /* 8.3 имя; путь с «0:/» после монтирования диска 0. */
    fr = f_open(&fil, "0:/FATFS1.TXT", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr == FR_OK) {
        printf("File opened successfully\n\r");
        static const char msg[] = "FatFs example 234324523523\n";
        f_write(&fil, msg, static_cast<UINT>(sizeof(msg) - 1u), &written);
        f_sync(&fil);
        f_close(&fil);
    }
    debugNUM("f_open: ", fr);

    f_mount(nullptr, board.SD.volumePath(), 0);
}
} // namespace

int main(void)
{
    board.serial1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
    board.serial1.open(9600);
    printf("Hello, World!\n");

    board.led.Off();
    fatfs_demo();
    printf("FatFs Done!\n");
    uint32_t last_blink_ms = 0;
    while (1) {
        const uint32_t now = board.GetTick();
        if ((now - last_blink_ms) >= 1000u) {
            last_blink_ms = now;
            board.led.Toggle();
            //printf("Blink: %d\n", now);
        }

        if (board.serial1.available()) {
            uint8_t data;
            board.serial1.read(&data, 1);
            board.serial1.write(&data, 1);
        }
    }
}