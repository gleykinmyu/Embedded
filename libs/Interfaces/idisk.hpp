#pragma once

#include <cstdint>

/**
 * FatFs: BIF::IDisk, BIF::DrvSlot, BIF::FatFsDiskVolumes (link / unlink), fatfs_disk_volumes.
 * Реализация disk_* — lib/Interfaces/diskio.cpp.
 */

#ifdef __cplusplus
extern "C" {
#endif
#include "../FatFs/diskio.h"
#include "../FatFs/ff.h"
#ifdef __cplusplus
}
#endif

namespace BIF {

class FatFsDiskVolumes;

class IDisk {
    friend class FatFsDiskVolumes;

public:
    virtual ~IDisk() = default;

    /// Путь тома FatFs «N:/» (4 байта); строка задаётся только из FatFsDiskVolumes::link (friend).
    const char* volumePath() const noexcept { return _path; }

    virtual DSTATUS initialize() = 0;
    virtual DSTATUS status()     = 0;
    virtual DRESULT read(BYTE* buff, DWORD sector, UINT count) = 0;

#if _USE_WRITE == 1
    virtual DRESULT write(const BYTE* buff, DWORD sector, UINT count) = 0;
#endif
#if _USE_IOCTL == 1
    virtual DRESULT ioctl(BYTE cmd, void* buff) = 0;
#endif

protected:
    /// LUN, переданный в IDisk(lun); для HAL/USB-мульти-LUN при необходимости.
    BYTE lun() const noexcept { return static_cast<BYTE>(_lun); }

    /// Регистрирует том в fatfs_disk_volumes (link + _path). Наследник: `: IDisk(lun)` в списке инициализации.
    explicit IDisk(BYTE lun = 0);

private:
    char     _path[4] = {};
    uint8_t  _lun = 0;
};

/**
 * Один логический том FatFs: драйвер и флаг однократного disk_initialize. LUN — в IDisk::_lun.
 */
struct DrvSlot {
    bool   isInitialized = false;
    IDisk* impl          = nullptr;

    bool slotOk() const noexcept { return impl != nullptr; }

    /// Первый вызов: помечает слот инициализированным и возвращает true; иначе false.
    bool consumeFirstInit() noexcept
    {
        if (isInitialized)
            return false;
        isInitialized = true;
        return true;
    }

    IDisk* idisk() const noexcept { return impl; }

    void bind(IDisk* iface) noexcept
    {
        isInitialized = false;
        impl          = iface;
    }

    void unbind() noexcept
    {
        isInitialized = false;
        impl          = nullptr;
    }
};

/**
 * Таблица томов: I/O через виртуальные методы выбранного слота.
 */
class FatFsDiskVolumes {
public:
    FatFsDiskVolumes() = default;
    FatFsDiskVolumes(const FatFsDiskVolumes&)            = delete; ///< Один глобальный экземпляр, копирование запрещено.
    FatFsDiskVolumes& operator=(const FatFsDiskVolumes&) = delete;

    /**
     * Зарегистрировать драйвер как следующий свободный том.
     * Пишет строку «N:/» в приватное поле iface->_path (доступ как friend).
     * @return true — успех, false — нет места или iface == nullptr
     */
    bool link(IDisk* iface);

    /**
     * Отвязать том по пути (первый символ — индекс диска, как в path после link).
     * Сдвигает хвост таблицы: индексы 0..n-1 остаются непрерывными, пути «N:/» пересчитываются.
     * @return true — успех, false — слот пуст или ошибка пути
     */
    bool unlink(char* path);

    /// Число сейчас привязанных томов.
    uint8_t attachedCount() const noexcept { return nbr_; }

    /// Указатель на IDisk для pdrv или nullptr, если индекс вне диапазона.
    IDisk* Driver(BYTE pdrv) const noexcept;

    /// true, если pdrv допустим и в слоте есть драйвер (можно вызывать read/write/…).
    bool slotOk(BYTE pdrv) const noexcept;

    /**
     * Логика однократного disk_initialize: первый вызов для pdrv помечает слот и возвращает true;
     * повторные — false (вызывающий не должен снова звать iface->initialize).
     * @return false также при pdrv >= _VOLUMES
     */
    bool consumeFirstInit(BYTE pdrv) noexcept;

private:
    DrvSlot          slots_[_VOLUMES]{};
    volatile uint8_t nbr_{};
};

/** Единственный экземпляр; определение в diskio.cpp */
extern FatFsDiskVolumes fatfs_disk_volumes;

} // namespace BIF
