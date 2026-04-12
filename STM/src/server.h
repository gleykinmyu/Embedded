#pragma once
#include "impl/serial.hpp"
#include "impl/sd_disk.hpp"
#include "STMboard.h"

//#include "nex.h"

class CServerBoard : public CBaseBoard 
{

public:
    PHL::Serial<PHL::ID::SERIAL1> serial1;
    PHL::Serial<PHL::ID::SERIAL2> serial2;
    PHL::SdDisk SD;

    CServerBoard() : CBaseBoard(), 
                        serial1(), 
                        serial2(), 
                        SD() {}

    //CHW_UART<USART2_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart2;
    //CHW_UART<USART3_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart3;
    //CHW_UART<UART4_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart4;
    //CHW_UART<UART5_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart5;
    //CHW_UART<USART6_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart6;
    

};

extern CServerBoard board;