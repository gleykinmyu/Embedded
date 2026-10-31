/**
 * @file device_caps.hpp
 * @brief Карта возможностей PT / Camera по модели (ConvertibleProtocol PDF).
 *
 * UI/HTTP прячут команды с --- у выбранной модели. Источник правды — здесь.
 */

#pragma once

#include "devices/device_types.hpp"

namespace ccam::devices {

/** Флаги P/T-команд (true = в матрице PDF есть поддержка). */
struct PtCaps {
    bool abs_pt = false;       ///< #APC
    bool abs_pt_speed = false; ///< #APS / #RPS
    bool preset_map = false;   ///< #PE
    bool limit_ctrl = false;   ///< #LC Set/Release
    bool limit_toggle = false; ///< #L1..4 toggle
    bool tally = false;
    bool fan = false;
    bool wiper = false;
    bool washer = false;
    bool wireless = false;
    bool option = false;
    bool tilt_range = false;
    bool heater = false;     ///< #HS
    bool defroster = false;  ///< #DS (PH350)
    bool pos_disp = false;   ///< #PTD
    bool pt_pos = false;     ///< #PTV / requestPosition pan+tilt (не PH: фейковый центр)
    bool lens_q = false;     ///< #AXZ/#AXF/#AXI (+ get) и/или Request #GZ/#GF/#GI
    bool exposure_q = false; ///< #LU и т.п.
    bool landing = false;
    bool software_ver = false;
    bool power_cam_tx = false; ///< отдельные #O CamTX-режимы (не PH)
};

/** Флаги camera-команд. */
struct CamCaps {
    bool hd_detail = false;   ///< ODT / HD detail (в основном HE870)
    bool neg_pos = false;
    bool field_frame = false;
    bool chroma = false;
    bool black_shade = false;
    bool synchro = false;
    bool abc = false; ///< ABC/ABB
    bool paint = false;
    bool genlock = false;
    bool gamma = false;
    bool agc_max = false;
    bool total_dtl = false;
    bool monitor = false;
    bool lens_h = false;
    bool menu_lr = false; ///< DLT/DRT Left/Right SW (HE120/HE130…)
};

/** Подпись для UI / NVS-ключа. */
const char* ptModelKey(PtModelId id);
const char* camModelKey(CameraModelId id);
const char* ptModelLabel(PtModelId id);
const char* camModelLabel(CameraModelId id);

PtModelId ptModelFromKey(const char* key);
CameraModelId camModelFromKey(const char* key);

PtCaps ptCapsFor(PtModelId id);
CamCaps camCapsFor(CameraModelId id);

} // namespace ccam::devices
