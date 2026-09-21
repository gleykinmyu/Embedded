#pragma once
#include <cstdint>
#include <type_traits>
#include <utility>
#include "config.h"
#include "core/gpio.h"
#include "phl.h"
#include "Debug.h"
#include <stm32f4xx_hal_sd.h>

#if defined(SD_DEBUG)
#  define SDIO_DETAIL_ERRORMSG(m) errorMSG(m)
#else
#  define SDIO_DETAIL_ERRORMSG(m) ((void)0)
#endif

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
    /** Пауза после тихой шины: VCC карты всегда 3V3, HAL внутри InitCard ждёт только 2 мс. */
    static constexpr uint32_t kCardSettleMs = 100u;

    static void init_sdio_gpios()
    {
        constexpr GPIO::AF af = PHL::IBase<SdioId>::af;
        /* CMD/DAT — pull-up по SD spec (Cube BSP). CK без подтяжки. */
        GPIO::PortC::pin<8>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<9>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<10>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<11>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        GPIO::PortC::pin<12>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        GPIO::PortD::pin<2>.Init(GPIO::ModeAlt::PP, af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        GPIO::PortD::pin<3>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
    }

    static void idle_sdio_gpios()
    {
        /* Тихая шина: карта, вставленная нагорячую, не видит бегущий CLK. */
        GPIO::PortC::pin<8>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        GPIO::PortC::pin<9>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        GPIO::PortC::pin<10>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        GPIO::PortC::pin<11>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        GPIO::PortC::pin<12>.Init(GPIO::Mode::Input, GPIO::Pull::Down);
        GPIO::PortD::pin<2>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        GPIO::PortD::pin<3>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
    }

    /* HAL_SD_DeInit: POWER=0, пустой MspDeInit, State=RESET. Пины/RCC — наши. */
    void stopPeripheral()
    {
        this->EnableClock();
        instance->POWER = 0u;
        this->Reset();
        hsd.ErrorCode = HAL_SD_ERROR_NONE;
        hsd.Context   = SD_CONTEXT_NONE;
        hsd.State     = HAL_SD_STATE_RESET;
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
        /* Transfer: SDIO_CK = 48 MHz / (0+2) = 24 MHz. Init ≤400 kHz — внутри HAL_SD_Init. */
        hsd.Init.ClockDiv            = 0u;
    }

    void busIdle()
    {
        stopPeripheral();
        idle_sdio_gpios();
        this->DisableClock();
    }

    CHW_Status Init()
    {
        CHW_Status sd_state = HAL_OK;
        SD_DBG("SDIO Init...\n");
        if (!IsDetected()) {
            SDIO_DETAIL_ERRORMSG("SD isn't present in slot.");
            if (hsd.State != HAL_SD_STATE_RESET) {
                busIdle();
            }
            return HAL_ERROR;
        }

        /* Сброс блока: после таймаута CMD CPSM/DPSM F4 часто клинит без RCC reset. */
        if (hsd.State != HAL_SD_STATE_RESET) {
            SD_DBG("SDIO re-init: stop first (state=%lu err=0x%lX)\n",
                static_cast<unsigned long>(hsd.State),
                static_cast<unsigned long>(hsd.ErrorCode));
            stopPeripheral();
        }

        /* Пауза на тихой шине (вход + pull-up), не в AF: при POWER=0 AF может держать CMD в 0. */
        idle_sdio_gpios();
        HAL_Delay(kCardSettleMs);

        this->Reset();
        this->EnableClock();
        this->init_sdio_gpios();
        sd_state = HAL_SD_Init(&hsd);
        if (sd_state != HAL_OK) {
            SDIO_DETAIL_ERRORMSG("Error HAL_SD_Init.");
            SD_DBG("SDIO Init FAIL err=0x%lX\n",
                static_cast<unsigned long>(hsd.ErrorCode));
            busIdle();
            return HAL_ERROR;
        }

        const bool wide4 =
            (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) == HAL_OK);
        if (!wide4) {
            /* Карта уже в 1 бите после HAL_SD_Init — не сносить сессию. */
            SD_DBG("SDIO 4B failed, stay 1B err=0x%lX\n",
                static_cast<unsigned long>(hsd.ErrorCode));
            SDIO_DETAIL_ERRORMSG("Error config bus wide 4B.");
        }
        SD_DBG("SDIO Init -> OK (4B=%s)\n", wide4 ? "yes" : "no");
        return HAL_OK;
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

    /** Карта в слоте: CD/PD3 = 0 (к корпусу). */
    bool IsDetected()
    {
        GPIO::PortD::pin<3>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        return !GPIO::PortD::pin<3>.Read();
    }
};

using CHW_SD = SD<PHL::ID::SDIO>;

} // namespace SDIO
