#include "server.h"
extern "C" {
#include "ff.h"
#include "ff_gen_drv.h"
}

#if defined(SDMMC_DATATIMEOUT)
#define SD_TIMEOUT SDMMC_DATATIMEOUT
#elif defined(SD_DATATIMEOUT)
#define SD_TIMEOUT SD_DATATIMEOUT
#else
#define SD_TIMEOUT 30 * 1000
#endif

#define SD_DEFAULT_BLOCK_SIZE 512

static DSTATUS _stat = STA_NOINIT;
static char _SDPath[4] = {}; 

extern "C" DSTATUS _CheckStatus(BYTE lun)
{
  _stat = STA_NOINIT;

  if(board.SD.GetCardState() == HAL_OK)
  {
    _stat &= ~STA_NOINIT;
  }

  return _stat;
}

/**
 * @brief  Initializes a Drive
 * @param  lun : not used
 * @retval DSTATUS: Operation status
 */
extern "C" DSTATUS ms_SD_initialize(BYTE lun)
{
  _stat = RES_ERROR;

  if (board.SD.Init() == HAL_OK)
  {
    _stat = RES_OK;
  }
  return _stat;
}


/**
 * @brief  Gets Disk Status
 * @param  lun : not used
 * @retval DSTATUS: Operation status
 */
extern "C" DSTATUS ms_SD_status(BYTE lun)
{
  return _CheckStatus(lun);
}


/**
 * @brief  Reads Sector(s)
 * @param  lun : not used
 * @param  *buff: Data buffer to store read data
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to read (1..128)
 * @retval DRESULT: Operation result
 */

extern "C" DRESULT ms_SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

  if (board.SD.ReadBlocks((uint32_t *)buff,
                        (uint32_t)(sector),
                        count, SD_TIMEOUT) == HAL_OK)
  {
    /* wait until the read operation is finished */
    while (board.SD.GetCardState() != HAL_SD_CARD_TRANSFER)
    {
    }
    res = RES_OK;
  }

  return res;
}

/**
 * @brief  Writes Sector(s)
 * @param  lun : not used
 * @param  *buff: Data to be written
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to write (1..128)
 * @retval DRESULT: Operation result
 */
#if _USE_WRITE == 1

extern "C" DRESULT ms_SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

  if (board.SD.WriteBlocks((uint32_t *)buff,
                         (uint32_t)(sector),
                         count, SD_TIMEOUT) == HAL_OK)
  {
    /* wait until the Write operation is finished */
    while (board.SD.GetCardState() != HAL_SD_CARD_TRANSFER)
    {
    }
    res = RES_OK;
  }

  return res;
}
#endif /* _USE_WRITE == 1 */

/**
 * @brief  I/O control operation
 * @param  lun : not used
 * @param  cmd: Control code
 * @param  *buff: Buffer to send/receive control data
 * @retval DRESULT: Operation result
 */
#if _USE_IOCTL == 1
extern "C" DRESULT ms_SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  CHW_SD::Info CardInfo;

  if (_stat & STA_NOINIT)
    return RES_NOTRDY;

  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC:
    res = RES_OK;
    break;

  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT:
    board.SD.GetCardInfo(&CardInfo);
    *(DWORD *)buff = CardInfo.LogBlockNbr;
    res = RES_OK;
    break;

  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE:
    board.SD.GetCardInfo(&CardInfo);
    *(WORD *)buff = CardInfo.LogBlockSize;
    res = RES_OK;
    break;

  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE:
    board.SD.GetCardInfo(&CardInfo);
    *(DWORD *)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
    res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}
#endif /* _USE_IOCTL == 1 */


const Diskio_drvTypeDef  ms_SD_Driver =
{
  ms_SD_initialize,
  ms_SD_status,
  ms_SD_read,
#if  _USE_WRITE == 1
  ms_SD_write,
#endif /* _USE_WRITE == 1 */

#if  _USE_IOCTL == 1
  ms_SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

void FATFS_LinkDrivers()
{
    if (FATFS_LinkDriver(&ms_SD_Driver, _SDPath))
    {
        errorMSG("FATFS cannot link drivers for SDIO.");
    };
}
