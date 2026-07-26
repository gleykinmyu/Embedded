#pragma once

#include <cstdint>

#include "impl/serial.hpp"

#include "impl/sd_disk.hpp"

#include "STMboard.h"



#include "nex.hpp"

#include "nexHmiConfig.hpp"



class CServerBoard : public CBaseBoard 

{



public:

    PHL::Serial<PHL::ID::SERIAL1, 2048, 64> serial1;

    PHL::Serial<PHL::ID::SERIAL2, 1024, 128> serial2;

    PHL::SdDisk SD;



    //CHW_UART<USART2_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart2;

    //CHW_UART<USART3_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart3;

    //CHW_UART<UART4_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart4;

    //CHW_UART<UART5_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart5;

    //CHW_UART<USART6_BASE, GPIOA_BASE, GPIO_PIN_9, GPIOA_BASE, GPIO_PIN_10, GPIO_AF7_USART1> uart6;

    



};



extern CServerBoard board;



/** HAL tick (мс) для `nex::Application` (`ClockMsFn`). */

uint32_t boardClockMs() noexcept;



/** Включить/выключить вывод `printf` на serial1 (не блокирует UART при off). */

void setSerial1LogEnabled(bool enabled) noexcept;



namespace server {



using namespace nex::comp;



/** Страница pgWork (panel page id 5), 47 виджетов id 1…47. */
struct PgWorkPage : nex::Page<47> {
    HMI_PAGE_CFG(pgWork);

    HMI_COMP(Hotspot, tc)
    HMI_COMP(Button<>, b0)
    HMI_COMP(Button<>, b1)
    HMI_COMP(Button<>, b2)
    HMI_COMP(Button<>, b3)
    HMI_COMP(Button<>, b4)
    HMI_COMP(Button<>, b5)
    HMI_COMP(Button<>, b6)
    HMI_COMP(Button<>, b7)
    HMI_COMP(Button<>, b8)
    HMI_COMP(Button<>, b9)
    HMI_COMP(Button<>, b10)
    HMI_COMP(Button<>, b11)
    HMI_COMP(Button<>, b12)
    HMI_COMP(Button<>, b13)
    HMI_COMP(Button<>, b14)
    HMI_COMP(Button<>, b15)
    HMI_COMP(Button<>, b16)
    HMI_COMP(Button<>, b17)
    HMI_COMP(Button<>, b18)
    HMI_COMP(Button<>, b19)
    HMI_COMP(Button<>, b20)
    HMI_COMP(Button<>, b21)
    HMI_COMP(Button<>, b22)
    HMI_COMP(Button<>, b23)
    HMI_COMP(Button<>, ctrlClear)
    HMI_COMP(Button<>, ctrlSelAll)
    HMI_COMP(Button<>, bS0)
    HMI_COMP(Button<>, bS1)
    HMI_COMP(Button<>, bS2)
    HMI_COMP(Button<>, bS3)
    HMI_COMP(Button<>, bS4)
    HMI_COMP(Button<>, bS5)
    HMI_COMP(Button<>, bS6)
    HMI_COMP(Button<>, bS7)
    HMI_COMP(Button<>, bSNext)
    HMI_COMP(Button<>, bSPrev)
    HMI_COMP(Button<>, mFile)
    HMI_COMP(Text<>, t0)
    HMI_COMP(Text<>, tSt)
    HMI_COMP(Text<>, tPg)
    HMI_COMP(DualStateButton<>, bsmScene)
    HMI_COMP(Timer, tmrBlock)
    HMI_COMP(Button<>, sBlock)
    HMI_COMP(StringVar<48>, varScnName)
    HMI_COMP(Text<>, t1)
    HMI_COMP(Slider<>, sShow)

    explicit PgWorkPage(nex::IAppUI& app) noexcept : Page<47>(app, "pgWork", PageCfg::kPageId) {}
};



} // namespace server


