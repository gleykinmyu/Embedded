/**
 * @file camera_commands.hpp
 * @brief Каталог команд Camera Control Protocol (ConvertibleProtocol.pdf v3.05).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/protocol_patterns.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::catalog {

struct CameraCommandEntry {
    const char* name;
    const char* query;
    const char* control;
    CameraPattern pattern;
};

/** Единая таблица: индекс = CameraCmd. */
inline constexpr CameraCommandEntry kCameraCommands[] = {
    {"Model Number", "QID", "OID", CameraPattern::SetO},
    {"Software Version", "QSV", "OSV", CameraPattern::SetO},
    {"AWC Mode", "QAW", "OAW", CameraPattern::SetO},
    {"Detail", "QDT", "ODT", CameraPattern::SetO},
    {"HD Detail", "QHD", "OHD", CameraPattern::SetO},
    {"Gain Up", "QGU", "OGU", CameraPattern::SetO},
    {"Shutter", "QSH", "OSH", CameraPattern::SetO},
    {"Synchro Scan", "QMS", "OMS", CameraPattern::SetO},
    {"Iris Auto/Manual", "QRS", "ORS", CameraPattern::SetO},
    {"Manual Iris Volume", "QRV", "ORV", CameraPattern::SetO},
    {"Picture Level", "QPV", "OPV", CameraPattern::SetO},
    {"Light Peak/Avg", "QPA", "OPA", CameraPattern::SetO},
    {"Light Area", "QAR", "OAR", CameraPattern::SetO},
    {"Neg/Pos", "QNP", "ONP", CameraPattern::SetO},
    {"R Pedestal", "QRD", "ORD", CameraPattern::SetO},
    {"B Pedestal", "QBD", "OBD", CameraPattern::SetO},
    {"R Gain", "QGR", "OGR", CameraPattern::SetO},
    {"B Gain", "QGB", "OGB", CameraPattern::SetO},
    {"T Pedestal", "QTD", "OTD", CameraPattern::SetO},
    {"H Phase", "QHP", "OHP", CameraPattern::SetO},
    {"SC Coarse", "QSC", "OSC", CameraPattern::SetO},
    {"SC Fine", "QSN", "OSN", CameraPattern::SetO},
    {"Chroma Level", "QCG", "OCG", CameraPattern::SetO},
    {"Scene File", "QSF", "OSF", CameraPattern::SetO},
    {"Field/Frame", "QFF", "OFR", CameraPattern::SetO},
    {"Auto Focus", "QAF", "OAF", CameraPattern::SetO},
    {"Digital Gain Up", "QDG", "ODG", CameraPattern::SetO},
    {"Digital Extender", "QDE", "ODE", CameraPattern::SetO},
    {"3D-DNR", "QDD", "ODD", CameraPattern::SetO},
    {"Color Bar", "QBR", "OBR", CameraPattern::SetO},
    {"Filter/ND", "QFT", "OFT", CameraPattern::SetO},
    {"Red Tally", "QLR", "OLR", CameraPattern::SetO},
    {"Green Tally", "QLG", "OLG", CameraPattern::SetO},
    {"T Pedestal (Video)", "QTP", "OTP", CameraPattern::SetO},
    {"R Gain (Video)", "QRI", "ORI", CameraPattern::SetO},
    {"B Gain (Video)", "QBI", "OBI", CameraPattern::SetO},
    {"R Pedestal (Video)", "QRP", "ORP", CameraPattern::SetO},
    {"B Pedestal (Video)", "QBP", "OBP", CameraPattern::SetO},
    {"Menu On/Off", "QUS", "OUS", CameraPattern::SetO},
    {"Bar Setup", "QCS", "OCS", CameraPattern::SetO},
    {"Zoom Speed (Lens I/F)", "QZS", "OZS", CameraPattern::SetO},
    {"Focus Speed (Lens I/F)", "QFS", "OFS", CameraPattern::SetO},
    {"Save Lens to Preset", "", "LPS", CameraPattern::SetO},
    {"Recall Lens Preset", "", "LPR", CameraPattern::SetO},
    {"Lens Focus Stop/Near/Far", "", "LFS", CameraPattern::SetO},
    {"AWC/AWB Set", "", "OWS", CameraPattern::Trigger},
    {"ABC/ABB Set", "", "OAS", CameraPattern::Trigger},
    {"Menu SW", "", "DPG", CameraPattern::SetO},
    {"Item SW", "", "DIT", CameraPattern::SetO},
    {"Yes SW", "", "DUP", CameraPattern::SetO},
    {"No SW", "", "DDW", CameraPattern::SetO},
    {"Pan Stop", "", "HPS", CameraPattern::Trigger},
    {"Pan Left", "", "HPL", CameraPattern::Trigger},
    {"Pan Right", "", "HPR", CameraPattern::Trigger},
    {"Tilt Stop", "", "HTS", CameraPattern::Trigger},
    {"Tilt Up", "", "HTU", CameraPattern::Trigger},
    {"Tilt Down", "", "HTD", CameraPattern::Trigger},
    {"Zoom Stop", "", "HZS", CameraPattern::Trigger},
    {"Zoom Wide", "", "HZW", CameraPattern::Trigger},
    {"Zoom Tele", "", "HZT", CameraPattern::Trigger},
    {"Focus Stop", "", "HFS", CameraPattern::Trigger},
    {"Focus Near", "", "HFN", CameraPattern::Trigger},
    {"Focus Far", "", "HFF", CameraPattern::Trigger},
};

inline constexpr size_t kCameraCommandCount = sizeof(kCameraCommands) / sizeof(kCameraCommands[0]);

static_assert(
    kCameraCommandCount == static_cast<size_t>(CameraCmd::Count),
    "kCameraCommands must match CameraCmd enum");

inline constexpr const CameraCommandEntry& getCameraCommand(CameraCmd cmd)
{
    return kCameraCommands[static_cast<size_t>(cmd)];
}

/** Pattern 3: [STX]XSF:n[ETX] — выбор сцены (PDF стр. 2). OSF — set Scene File (CameraCmd::SceneFile). */
inline constexpr CameraCommandEntry kSceneCommand = {
    "Scene File Select", "QSF", "XSF", CameraPattern::Scene};

static_assert(kSceneCommand.pattern == CameraPattern::Scene, "kSceneCommand: Pattern 3 Scene");
static_assert(
    kSceneCommand.control[0] == 'X' && kSceneCommand.control[1] == 'S' &&
        kSceneCommand.control[2] == 'F' && kSceneCommand.control[3] == '\0',
    "kSceneCommand: control must be XSF");

/** Pattern 4: Dxx:n */
inline constexpr CameraCommandEntry kMonitorPrefix = {
    "Monitor", "", "D", CameraPattern::Monitor};

/** Pattern 8: Hxx */
inline constexpr CameraCommandEntry kLensHPrefix = {
    "Lens Contact Closer", "", "H", CameraPattern::LensH};

} // namespace ccam::catalog
