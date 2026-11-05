/**
 * @file cam_menu_deps.hpp
 * @brief Каталог опроса вкладки Camera: primary → deps → always.
 *
 * Слоты ссылаются на существующие CameraCmd / OsdItem / VideoMenuItem.
 * CamMenuFmt — только как читать/отдать JSON (не второй каталог параметров).
 */

#pragma once

#include "catalog/camera_commands.hpp"
#include "catalog/osd_menu.hpp"
#include "catalog/video_menu.hpp"
#include "devices/cam_param_ranges.hpp"
#include "devices/device_caps.hpp"
#include "devices/device_types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::devices {

enum class CamMenuPhase : uint8_t {
    Primary = 1,
    Dep = 2,
    Always = 3,
};

enum class CamMenuCap : uint8_t {
    None = 0,
    Chroma,
    Paint,
    Synchro,
    AgcMax,
    Gamma,
    HdDetail,
    TotalDtl,
    Genlock,
    NegPos,
    FieldFrame,
};

/** Банк протокола — те же enum’ы, что в catalog/. */
enum class CamMenuBank : uint8_t {
    Cam = 0,   ///< catalog::CameraCmd
    Osd = 1,   ///< catalog::OsdItem
    Video = 2, ///< catalog::VideoMenuItem
};

/**
 * Формат ответа / побочный эффект primary.
 * Не дублирует имена параметров — описывает codec слоя HTTP.
 */
enum class CamMenuFmt : uint8_t {
    DecU8 = 0,
    HexU16,
    Switch,
    IrisAm,       ///< ORS + pst.iris_am
    ColorBar,
    FieldFrame,
    GainMode,     ///< QGU → gain_mode[/gain_db] + pst.gain_agc
    GainDb,       ///< QGU → только gain_db (deps)
    IrisNorm,     ///< ORV → 0..FFFF
    Synchro,      ///< QMS hex u16
    ChromaSigned, ///< QCG → −3…+3
    AirisSigned,  ///< OSD:48 → −50…+50
    OsdWire,      ///< строка data после ':'
    OsdHexU,      ///< hex → unsigned
    VideoWire,
    Model,
    Version,
    TrackShutter, ///< DecU8 + pst.shutter
    TrackAwc,     ///< DecU8 + pst.awc
};

struct CamMenuPollEntry {
    const char* key;
    CamMenuPhase phase;
    const char* gate;
    CamMenuCap cap;
    CamMenuBank bank;
    uint8_t item; ///< CameraCmd | OsdItem | VideoMenuItem
    CamMenuFmt fmt;
};

struct CamMenuPrimaryState {
    bool gain_agc = false;
    uint8_t shutter = 0;
    uint8_t iris_am = 0;
    uint8_t awc = 0;
    bool he_awc = false;
    uint8_t shutter_synchro = 0x0B;
};

const CamMenuPollEntry* camMenuPoll(size_t& count);
const CamMenuPollEntry* camMenuDeps(size_t& count);

bool camMenuGateActive(const char* gate, const CamMenuPrimaryState& st);
bool camMenuCapOk(CamMenuCap cap, const CamCaps& caps);
bool camMenuEntryWanted(
    const CamMenuPollEntry& e,
    const CamMenuPrimaryState& st,
    const CamCaps& caps,
    const char* only_csv,
    const char* gates_csv);

int camMenuDepsToJson(char* out, size_t out_cap);

inline bool camMenuUsesHeAwc(CameraModelId id)
{
    return camUsesHeAgc(id);
}

inline constexpr uint8_t camMenuItem(catalog::CameraCmd c)
{
    return static_cast<uint8_t>(c);
}
inline constexpr uint8_t camMenuItem(catalog::OsdItem c)
{
    return static_cast<uint8_t>(c);
}
inline constexpr uint8_t camMenuItem(catalog::VideoMenuItem c)
{
    return static_cast<uint8_t>(c);
}

} // namespace ccam::devices
