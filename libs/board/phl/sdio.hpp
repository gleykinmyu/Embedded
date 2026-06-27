#pragma once
#include <cstdint>
#include <type_traits>
#include <utility>
#include "config.h"
#include "core/gpio.h"
#include "phl.h"
#include "Debug.h"
#include <stm32f4xx_hal_sd.h>

#define SDIO_DETAIL_ERRORMSG(m) errorMSG(m)

namespace SDIO {

/**
 * SDIO/SDMMC + HAL SD: PHL::ID, IBase (тактирование, AF), InitPins как у UART::UART::InitPins.
 * Экземпляр периферии — hsd.Instance; операции блоков — через HAL_SD_*.
 */
template <PHL::ID SdioId>
class SD : public PHL::IBase<SdioId> {
    static_assert(PHL::GetType<SdioId>::value == PHL::Type::SDIO,
                  "SDIO::SD: только PHL::ID с Type::SDIO");
private:
    static void init_sdio_gpios()
    {
        constexpr GPIO::AF af = PHL::IBase<SdioId>::af;
        GPIO::PortC::pin<8>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<9>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<10>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<11>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<12>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        GPIO::PortD::pin<2>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
    }

public:
    using InstanceType =
        std::remove_pointer_t<decltype(std::declval<SD_HandleTypeDef>().Instance)>;
    using Info  = HAL_SD_CardInfoTypeDef;
    using State = HAL_SD_CardStateTypeDef;

    InstanceType* const instance;
    SD_HandleTypeDef    hsd = {};

    SD() : instance(reinterpret_cast<InstanceType*>(static_cast<uintptr_t>(SdioId)))
    {
        hsd.Instance                 = instance;
        hsd.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
        hsd.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
        hsd.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
        hsd.Init.BusWide             = SDIO_BUS_WIDE_1B;
        hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
        hsd.Init.ClockDiv            = 0;
    }

    CHW_Status Init()
    {
        CHW_Status sd_state = HAL_OK;
        if (!IsDetected()) {
            SDIO_DETAIL_ERRORMSG("SD isn't present in slot.");
            return HAL_ERROR;
        }
        this->EnableClock();
        this->init_sdio_gpios();
        sd_state = HAL_SD_Init(&hsd);
        if (sd_state != HAL_OK)
            SDIO_DETAIL_ERRORMSG("Error HAL_SD_Init.");

        if (sd_state == HAL_OK) {
            if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK) {
                SDIO_DETAIL_ERRORMSG("Error config bus wide 4B.");
                sd_state = HAL_ERROR;
            }
        }
        return sd_state;
    }

    CHW_Status ConfigWideBus(uint32_t flag) { return HAL_SD_ConfigWideBusOperation(&hsd, flag); }

    CHW_Status ReadBlocks(uint32_t* pData, uint32_t read_addr, uint32_t num_blocks,
                          uint32_t timeout = 0xFFFF)
    {
        const CHW_Status st =
            HAL_SD_ReadBlocks(&hsd, reinterpret_cast<uint8_t*>(pData), read_addr, num_blocks, timeout);
        if (st != HAL_OK)
            SDIO_DETAIL_ERRORMSG("Error Read Blocks.");
        return st;
    }

    CHW_Status WriteBlocks(uint32_t* pData, uint32_t write_addr, uint32_t num_blocks,
                           uint32_t timeout = 0xFFFF)
    {
        const CHW_Status st =
            HAL_SD_WriteBlocks(&hsd, reinterpret_cast<uint8_t*>(pData), write_addr, num_blocks, timeout);
        if (st != HAL_OK)
            SDIO_DETAIL_ERRORMSG("Error Write Blocks.");
        return st;
    }

    CHW_Status Erase(uint32_t start_addr, uint32_t end_addr)
    {
        const CHW_Status st = HAL_SD_Erase(&hsd, start_addr, end_addr);
        if (st != HAL_OK)
            SDIO_DETAIL_ERRORMSG("Error Erase SD Card.");
        return st;
    }

    State GetCardState() { return HAL_SD_GetCardState(&hsd); }

    CHW_Status GetCardInfo(Info* info)
    {
        const CHW_Status st = HAL_SD_GetCardInfo(&hsd, info);
        if (st != HAL_OK)
            SDIO_DETAIL_ERRORMSG("Error Get Card Info.");
        return st;
    }

    bool IsDetected() { return true; }
};

using CHW_SD = SD<PHL::ID::SDIO>;

} // namespace SDIO
