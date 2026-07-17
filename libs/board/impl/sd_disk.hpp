#pragma once
#include "idisk.hpp"
#include "phl/sdio.hpp"
#include <stm32f4xx_hal.h>

namespace PHL {

/* Софт-таймаут HAL_SD_*Blocks / waitUntilTransfer (мс).
 * Не путать с SDMMC_DATATIMEOUT из Cube (на F4 = 0xFFFFFFFF). */
#ifndef SD_DATATIMEOUT
#define SD_DATATIMEOUT 1000u
#endif
constexpr uint32_t kSdDiskTimeout = SD_DATATIMEOUT;

constexpr uint32_t kSdDefaultBlockSize = 512u;

/** Ожидание перехода карты в TRANSFER после блоковой операции HAL (не блокировать навсегда). */
constexpr uint32_t kSdWaitTransferTimeoutMs = kSdDiskTimeout;

/**
 * BIF::IDisk поверх единственного на плате SDIO::SD<PHL::ID::SDIO>.
 * Регистрация FatFs — в базовом IDisk(lun) в списке инициализации.
 */
class SdDisk : public BIF::IDisk {
    using HwSd     = SDIO::SD<PHL::ID::SDIO>;
    using CardInfo = HwSd::Info;

    HwSd    _sd{};
    DSTATUS _stat = STA_NOINIT;

    bool waitUntilTransfer(uint32_t timeout_ms) noexcept
    {
        const uint32_t start = HAL_GetTick();
        while (_sd.GetCardState() != HAL_SD_CARD_TRANSFER) {
            if ((HAL_GetTick() - start) >= timeout_ms) {
                _stat = STA_NOINIT;
                return false;
            }
        }
        return true;
    }

public:
    SdDisk() : IDisk(0) {}

    DSTATUS initialize() override
    {
        /* Уже готов — не трогаем HAL (FatFs может звать disk_initialize повторно). */
        if ((_stat & STA_NOINIT) == 0) {
            return status();
        }

        if (_sd.Init() != HAL_OK) {
            _stat = STA_NOINIT;
            if (!_sd.IsDetected()) {
                _stat = static_cast<DSTATUS>(STA_NOINIT | STA_NODISK);
            }
            return _stat;
        }
        _stat = 0;
        return 0;
    }

    DSTATUS status() override
    {
        if (_stat & STA_NOINIT) {
            return _stat;
        }
        /* GetCardState() — состояние карты (HAL_SD_CARD_*), не HAL_StatusTypeDef. */
        const auto card = _sd.GetCardState();
        if (card == HAL_SD_CARD_ERROR) {
            _stat = STA_NOINIT;
            return STA_NOINIT;
        }
        return 0;
    }

    DRESULT read(BYTE* buff, DWORD sector, UINT count) override
    {
        if (status() & STA_NOINIT) {
            return RES_NOTRDY;
        }
        DRESULT res = RES_ERROR;
        if (_sd.ReadBlocks(reinterpret_cast<uint32_t*>(buff), static_cast<uint32_t>(sector), count,
                           kSdDiskTimeout) == HAL_OK) {
            if (waitUntilTransfer(kSdWaitTransferTimeoutMs))
                res = RES_OK;
        }
        if (res != RES_OK) {
            _stat = STA_NOINIT;
        }
        return res;
    }

#if _USE_WRITE == 1
    DRESULT write(const BYTE* buff, DWORD sector, UINT count) override
    {
        if (status() & STA_NOINIT) {
            return RES_NOTRDY;
        }
        DRESULT res = RES_ERROR;
        if (_sd.WriteBlocks(reinterpret_cast<uint32_t*>(const_cast<BYTE*>(buff)),
                            static_cast<uint32_t>(sector), count, kSdDiskTimeout) == HAL_OK) {
            if (waitUntilTransfer(kSdWaitTransferTimeoutMs))
                res = RES_OK;
        }
        if (res != RES_OK) {
            _stat = STA_NOINIT;
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
