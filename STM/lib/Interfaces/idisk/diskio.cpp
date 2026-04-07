/*-----------------------------------------------------------------------*/
/* Реализация API diskio.h: вызовы виртуальных методов BIF::IDisk.       */
/*-----------------------------------------------------------------------*/

#include "../idisk.hpp"

#if defined(__GNUC__) && !defined(__weak)
#define __weak __attribute__((weak))
#endif

namespace BIF {
FatFsDiskVolumes fatfs_disk_volumes;
}

extern "C" {

DSTATUS disk_status(BYTE pdrv)
{
    if (!BIF::fatfs_disk_volumes.slotOk(pdrv))
        return STA_NOINIT;
    return BIF::fatfs_disk_volumes.Driver(pdrv)->status();
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (!BIF::fatfs_disk_volumes.slotOk(pdrv))
        return STA_NOINIT;
    if (!BIF::fatfs_disk_volumes.consumeFirstInit(pdrv))
        return 0;
    return BIF::fatfs_disk_volumes.Driver(pdrv)->initialize();
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    if (!BIF::fatfs_disk_volumes.slotOk(pdrv))
        return RES_ERROR;
    return BIF::fatfs_disk_volumes.Driver(pdrv)->read(buff, sector, count);
}

#if _USE_WRITE == 1
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    if (!BIF::fatfs_disk_volumes.slotOk(pdrv))
        return RES_ERROR;
    return BIF::fatfs_disk_volumes.Driver(pdrv)->write(buff, sector, count);
}
#endif

#if _USE_IOCTL == 1
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    if (!BIF::fatfs_disk_volumes.slotOk(pdrv))
        return RES_ERROR;
    return BIF::fatfs_disk_volumes.Driver(pdrv)->ioctl(cmd, buff);
}
#endif

__weak DWORD get_fattime(void)
{
    return 0;
}

} // extern "C"
