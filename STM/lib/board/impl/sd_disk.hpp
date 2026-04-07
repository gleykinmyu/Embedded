#pragma once
#include "../../Interfaces/idisk.hpp"
#include "../phl/sdio.hpp"
#include <stm32f4xx_hal.h>

namespace PHL {

#if defined(SDMMC_DATATIMEOUT)
constexpr uint32_t kSdDiskTimeout = SDMMC_DATATIMEOUT;
#elif defined(SD_DATATIMEOUT)
constexpr uint32_t kSdDiskTimeout = SD_DATATIMEOUT;
#else
constexpr uint32_t kSdDiskTimeout = 30u * 1000u;
#endif

constexpr uint32_t kSdDefaultBlockSize = 512u;

/**
 * BIF::IDisk поверх единственного на плате SDIO::SD<PHL::ID::SDIO>.
 * Регистрация FatFs — в базовом IDisk(lun) в списке инициализации.
 */
class SdDisk : public BIF::IDisk {
    using HwSd     = SDIO::SD<PHL::ID::SDIO>;
    using CardInfo = HwSd::Info;

    HwSd    _sd{};
    DSTATUS _stat = STA_NOINIT;

public:
    SdDisk() : IDisk(0) {}

    DSTATUS initialize() override
    {
        if (_sd.Init() != HAL_OK) {
            _stat = STA_NOINIT;
            return STA_NOINIT;
        }
        _stat = 0;
        return 0;
    }

    DSTATUS status() override
    {
        /* GetCardState() — состояние карты (HAL_SD_CARD_*), не HAL_StatusTypeDef. */
        if (_sd.GetCardState() == HAL_SD_CARD_ERROR) {
            _stat = STA_NOINIT;
            return STA_NOINIT;
        }
        _stat = 0;
        return 0;
    }

    DRESULT read(BYTE* buff, DWORD sector, UINT count) override
    {
        DRESULT res = RES_ERROR;
        if (_sd.ReadBlocks(reinterpret_cast<uint32_t*>(buff), static_cast<uint32_t>(sector), count,
                           kSdDiskTimeout) == HAL_OK) {
            while (_sd.GetCardState() != HAL_SD_CARD_TRANSFER) {
            }
            res = RES_OK;
        }
        return res;
    }

#if _USE_WRITE == 1
    DRESULT write(const BYTE* buff, DWORD sector, UINT count) override
    {
        DRESULT res = RES_ERROR;
        if (_sd.WriteBlocks(reinterpret_cast<uint32_t*>(const_cast<BYTE*>(buff)),
                            static_cast<uint32_t>(sector), count, kSdDiskTimeout) == HAL_OK) {
            while (_sd.GetCardState() != HAL_SD_CARD_TRANSFER) {
            }
            res = RES_OK;
        }
        return res;
    }
#endif

#if _USE_IOCTL == 1
    DRESULT ioctl(BYTE cmd, void* buff) override
    {
        DRESULT  res       = RES_ERROR;
        CardInfo card_info{};

        if (status() & STA_NOINIT)
            return RES_NOTRDY;

        switch (cmd) {
        case CTRL_SYNC:
            res = RES_OK;
            break;
        case GET_SECTOR_COUNT:
            _sd.GetCardInfo(&card_info);
            *reinterpret_cast<DWORD*>(buff) = card_info.LogBlockNbr;
            res                               = RES_OK;
            break;
        case GET_SECTOR_SIZE:
            _sd.GetCardInfo(&card_info);
            *reinterpret_cast<WORD*>(buff) = static_cast<WORD>(card_info.LogBlockSize);
            res                            = RES_OK;
            break;
        case GET_BLOCK_SIZE:
            _sd.GetCardInfo(&card_info);
            *reinterpret_cast<DWORD*>(buff) = card_info.LogBlockSize / kSdDefaultBlockSize;
            res                               = RES_OK;
            break;
        default:
            res = RES_PARERR;
        }
        return res;
    }
#endif
};

} // namespace PHL
