#pragma once

#include <cstdint>

/** Решение перед следующим обменом уже начатой многошаговой операции. */
enum class DeviceGuard : uint8_t { Ok, EpochChanged, OtaBusy };

/**
 * Поколение важнее OTA: после смены объекта результаты прошлого устройства
 * применять нельзя, даже если обновление уже началось.
 */
inline DeviceGuard device_guard(uint32_t epoch_at_start, uint32_t epoch_now, bool ota_busy)
{
    if (epoch_now != epoch_at_start) {
        return DeviceGuard::EpochChanged;
    }
    if (ota_busy) {
        return DeviceGuard::OtaBusy;
    }
    return DeviceGuard::Ok;
}
