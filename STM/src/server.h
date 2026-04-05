#pragma once
#include "stmboard.h"
#include "phl/serial.hpp"
//#include "nex.h"

class CServerBoard : public CBaseBoard 
{

public:
    PHL::Serial<PHL::ID::SERIAL1> uart1;

    CServerBoard() : CBaseBoard()
    {
        uart1.InitPins(GPIO::PortA::pin<9>, GPIO::PortA::pin<10>);
        (void)uart1.open(9600);

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