#include "../idisk.hpp"
#include "Debug.h"

namespace BIF {

IDisk::IDisk(BYTE lun) : _lun(static_cast<uint8_t>(lun))
{
    if (!fatfs_disk_volumes.link(this))
        errorMSG("FATFS cannot link disk volume.");
}

IDisk* FatFsDiskVolumes::Driver(BYTE pdrv) const noexcept
{
    if (pdrv >= _VOLUMES)
        return nullptr;
    return slots_[pdrv].idisk();
}

bool FatFsDiskVolumes::slotOk(BYTE pdrv) const noexcept
{
    if (pdrv >= _VOLUMES)
        return false;
    return slots_[pdrv].slotOk();
}

bool FatFsDiskVolumes::consumeFirstInit(BYTE pdrv) noexcept
{
    if (pdrv >= _VOLUMES)
        return false;
    return slots_[pdrv].consumeFirstInit();
}

bool FatFsDiskVolumes::link(IDisk* iface)
{
    if (nbr_ >= _VOLUMES || iface == nullptr)
        return false;

    const uint8_t idx = nbr_;
    slots_[idx].bind(iface);
    const uint8_t disk_num = nbr_++;
    iface->_path[0] = static_cast<char>(disk_num + '0');
    iface->_path[1] = ':';
    iface->_path[2] = '/';
    iface->_path[3] = 0;
    return true;
}

bool FatFsDiskVolumes::unlink(char* path)
{
    if (path == nullptr)
        return false;

    const uint8_t n = nbr_;
    if (n < 1u)
        return false;

    const uint8_t disk_num = static_cast<uint8_t>(path[0] - '0');
    if (disk_num >= _VOLUMES || disk_num >= n || !slots_[disk_num].slotOk())
        return false;

    /* Сдвиг влево: без дыр в slots_[0..n-2], перенумерация путей FatFs «0:/», «1:/», … */
    for (uint8_t i = disk_num; static_cast<uint16_t>(i) + 1u < static_cast<uint16_t>(n); ++i) {
        slots_[i] = slots_[i + 1u];
        if (IDisk* iface = slots_[i].impl) {
            iface->_path[0] = static_cast<char>('0' + i);
            iface->_path[1] = ':';
            iface->_path[2] = '/';
            iface->_path[3] = 0;
        }
    }
    slots_[static_cast<uint8_t>(n - 1u)].unbind();
    nbr_ = static_cast<uint8_t>(n - 1u);
    return true;
}

} // namespace BIF
