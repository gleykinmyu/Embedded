/**
 * @file cam_param_ranges.hpp
 * @brief Каталог UI-диапазонов camera-параметров по модели (ConvertibleProtocol).
 *
 * Источник правды для web/CLI — не хардкод в HTML.
 * Choices: wire = строка для HTTP (v/data); HexRange: min..max → hex digits.
 */

#pragma once

#include "devices/device_types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::devices {

struct CamChoice {
    const char* wire;  ///< значение в API (v или data)
    const char* label; ///< подпись UI
};

enum class CamParamKind : uint8_t {
    Choices = 0,
    HexRange = 1, ///< min..max включительно, wire = hex с hex_digits
};

struct CamParamSpec {
    CamParamKind kind = CamParamKind::Choices;
    const CamChoice* choices = nullptr;
    size_t choice_count = 0;
    uint32_t min_u = 0;
    uint32_t max_u = 0;
    uint8_t hex_digits = 0;
};

/** Набор параметров, которые UI заполняет из API. */
struct CamParamCatalog {
    CamParamSpec awc{};
    CamParamSpec scene{};
    CamParamSpec shutter{};
    CamParamSpec synchro{};
    CamParamSpec gamma{};
    CamParamSpec agc_max{};
    CamParamSpec total_dtl{};
    CamParamSpec genlock{};
    CamParamSpec colorbar{};
};

/**
 * HE-шкала AGC: HE130/HE870/UE150 и HE120/HE60/HE50.
 * Короткий AWC (Var=5) — только у моделей с He/Ue AwcMode, не у HE120.
 */
inline bool camUsesHeAgc(CameraModelId id)
{
    switch (id) {
    case CameraModelId::He130:
    case CameraModelId::He870:
    case CameraModelId::Ue150:
    case CameraModelId::He120:
    case CameraModelId::He60:
    case CameraModelId::He50:
        return true;
    default:
        return false;
    }
}

/** Каталог для модели (Generic → E650-подобный studio fallback). */
const CamParamCatalog& camParamCatalog(CameraModelId id);

/**
 * Сериализация catalog → JSON-объект (без внешних `{}`).
 * Пример: `"shutter":{"kind":"choices","items":[...]},"synchro":{...}`
 * @return байт записано, или -1 при ошибке / переполнении.
 */
int camParamCatalogToJson(const CamParamCatalog& cat, char* out, size_t out_cap);

} // namespace ccam::devices
