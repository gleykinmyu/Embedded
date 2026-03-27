#include <stdio.h>
#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include <string.h>
#include "server.h"


CServerBoard board;

uint8_t UART1_rxBuffer[12] = {0};



int main(void)
{
  FATFS_LinkDrivers();

  board.led.Off();

  printf("Start\n");
  //board.uart1.Read_IT(UART1_rxBuffer, 10);


  while (1)
  {
    board.led.Toggle();
    //printf("Testing!\n");
    //printf("Time: %lu\n", board.GetTick());
    board.Delay(1000);
  }
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    //board.uart1.Write(UART1_rxBuffer, 10, 0);
    //board.uart1.Read_IT(UART1_rxBuffer, 10);
}