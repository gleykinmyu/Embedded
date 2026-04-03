#pragma once
#include "stmboard.h"
#include "nex.h"

class CServerBoard : public CBaseBoard 
{

public:
    CServerBoard() : CBaseBoard()
    {
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);

        //uart1.Begin(9600);
        //uart1.IRQ.SetPriority(0, 0);
        //uart1.IRQ.Enable();

        SD.Init();
    };

    //CHW_UART<USART2_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart2;
    //CHW_UART<USART3_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart3;
    //CHW_UART<UART4_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart4;
    //CHW_UART<UART5_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart5;
    //CHW_UART<USART6_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart6;
    

};

extern CServerBoard board;

void FATFS_LinkDrivers();