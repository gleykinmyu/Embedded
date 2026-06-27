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

    HMI_WIDGET(Hotspot, tc)
    HMI_WIDGET(Button<>, b0)
    HMI_WIDGET(Button<>, b1)
    HMI_WIDGET(Button<>, b2)
    HMI_WIDGET(Button<>, b3)
    HMI_WIDGET(Button<>, b4)
    HMI_WIDGET(Button<>, b5)
    HMI_WIDGET(Button<>, b6)
    HMI_WIDGET(Button<>, b7)
    HMI_WIDGET(Button<>, b8)
    HMI_WIDGET(Button<>, b9)
    HMI_WIDGET(Button<>, b10)
    HMI_WIDGET(Button<>, b11)
    HMI_WIDGET(Button<>, b12)
    HMI_WIDGET(Button<>, b13)
    HMI_WIDGET(Button<>, b14)
    HMI_WIDGET(Button<>, b15)
    HMI_WIDGET(Button<>, b16)
    HMI_WIDGET(Button<>, b17)
    HMI_WIDGET(Button<>, b18)
    HMI_WIDGET(Button<>, b19)
    HMI_WIDGET(Button<>, b20)
    HMI_WIDGET(Button<>, b21)
    HMI_WIDGET(Button<>, b22)
    HMI_WIDGET(Button<>, b23)
    HMI_WIDGET(Button<>, ctrlClear)
    HMI_WIDGET(Button<>, ctrlSelAll)
    HMI_WIDGET(Button<>, bS0)
    HMI_WIDGET(Button<>, bS1)
    HMI_WIDGET(Button<>, bS2)
    HMI_WIDGET(Button<>, bS3)
    HMI_WIDGET(Button<>, bS4)
    HMI_WIDGET(Button<>, bS5)
    HMI_WIDGET(Button<>, bS6)
    HMI_WIDGET(Button<>, bS7)
    HMI_WIDGET(Button<>, bSNext)
    HMI_WIDGET(Button<>, bSPrev)
    HMI_WIDGET(Button<>, mFile)
    HMI_WIDGET(Text<>, t0)
    HMI_WIDGET(Text<>, tSt)
    HMI_WIDGET(Text<>, tPg)
    HMI_WIDGET(DualStateButton<>, bsmScene)
    HMI_WIDGET(Timer, tmrBlock)
    HMI_WIDGET(Button<>, sBlock)
    HMI_WIDGET(StringVar<48>, varScnName)
    HMI_WIDGET(Text<>, t1)
    HMI_WIDGET(Slider<>, sShow)

    explicit PgWorkPage(nex::IAppUI& app) noexcept : Page<47>(app, "pgWork", PageCfg::kPageId) {}
};



} // namespace server


