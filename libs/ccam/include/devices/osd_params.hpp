/**
 * @file osd_params.hpp
 * @brief User OSD: именованный set/get и диапазон из ConvertibleProtocol.pdf v3.05.
 *
 * Каждый ключ — отдельный параметр. set проверяет код по шкале PDF,
 * get читает QSD и возвращает payload OSD:nn:data.
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "devices/cam_param_ranges.hpp"
#include "devices/device_types.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::devices {

class CameraDeviceBase;

struct OsdParam {
    catalog::OsdItem item;
    const char* key;
    const char* group;
    const char* name;
    const char* note;
    bool linear;
    uint8_t min_u;
    uint8_t max_u;
    int16_t bias;
    const char* unit;
    const char* end_lo;
    const char* end_hi;
    const CamChoice* choices;
    uint8_t choice_count;
    bool agc;
};

const OsdParam* osdParamByKey(const char* key);
const OsdParam* osdParamTable(size_t& count);

/** Канонический data (2 hex) или nullptr, если код вне шкалы. */
bool osdParamWire(const OsdParam& param, CameraModelId model, const char* wire, char out[3]);

ccam::Status setOsdParam(CameraDeviceBase& cam, const char* key, const char* wire);
ccam::Status getOsdParam(CameraDeviceBase& cam, const char* key, char* out, size_t out_cap);

/** JSON-массив параметров (с `[` `]`). */
int osdParamCatalogToJson(CameraModelId model, char* out, size_t out_cap);

} // namespace ccam::devices
