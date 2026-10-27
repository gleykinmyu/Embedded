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
    const char* query;
    const char* control;
    CameraPattern pattern;
};

/** Единая таблица: индекс = CameraCmd. */
inline constexpr CameraCommandEntry kCameraCommands[] = {
    {"QID", "OID", CameraPattern::SetO},
    {"QSV", "OSV", CameraPattern::SetO},
    {"QAW", "OAW", CameraPattern::SetO},
    {"QDT", "ODT", CameraPattern::SetO},
    {"QHD", "OHD", CameraPattern::SetO},
    {"QGU", "OGU", CameraPattern::SetO},
    {"QSH", "OSH", CameraPattern::SetO},
    {"QMS", "OMS", CameraPattern::SetO},
    {"QRS", "ORS", CameraPattern::SetO},
    {"QRV", "ORV", CameraPattern::SetO},
    {"QPV", "OPV", CameraPattern::SetO},
    {"QPA", "OPA", CameraPattern::SetO},
    {"QAR", "OAR", CameraPattern::SetO},
    {"QNP", "ONP", CameraPattern::SetO},
    {"QRD", "ORD", CameraPattern::SetO},
    {"QBD", "OBD", CameraPattern::SetO},
    {"QGR", "OGR", CameraPattern::SetO},
    {"QGB", "OGB", CameraPattern::SetO},
    {"QTD", "OTD", CameraPattern::SetO},
    {"QHP", "OHP", CameraPattern::SetO},
    {"QSC", "OSC", CameraPattern::SetO},
    {"QSN", "OSN", CameraPattern::SetO},
    {"QCG", "OCG", CameraPattern::SetO},
    {"QSF", "OSF", CameraPattern::SetO},
    {"QFF", "OFR", CameraPattern::SetO},
    {"QAF", "OAF", CameraPattern::SetO},
    {"QDG", "ODG", CameraPattern::SetO},
    {"QDE", "ODE", CameraPattern::SetO},
    {"QDD", "ODD", CameraPattern::SetO},
    {"QBR", "OBR", CameraPattern::SetO},
    {"QFT", "OFT", CameraPattern::SetO},
    {"QLR", "OLR", CameraPattern::SetO},
    {"QLG", "OLG", CameraPattern::SetO},
    {"QTP", "OTP", CameraPattern::SetO},
    {"QRI", "ORI", CameraPattern::SetO},
    {"QBI", "OBI", CameraPattern::SetO},
    {"QRP", "ORP", CameraPattern::SetO},
    {"QBP", "OBP", CameraPattern::SetO},
    {"QUS", "OUS", CameraPattern::SetO},
    {"QCS", "OCS", CameraPattern::SetO},
    {"QZS", "OZS", CameraPattern::SetO},
    {"QFS", "OFS", CameraPattern::SetO},
    {"", "LPS", CameraPattern::SetO},
    {"", "LPR", CameraPattern::SetO},
    {"", "LFS", CameraPattern::SetO},
    {"", "OWS", CameraPattern::Trigger},
    {"", "OAS", CameraPattern::Trigger},
    {"", "DPG", CameraPattern::SetO},
    {"", "DIT", CameraPattern::SetO},
    {"", "DUP", CameraPattern::SetO},
    {"", "DDW", CameraPattern::SetO},
    {"", "HPS", CameraPattern::Trigger},
    {"", "HPL", CameraPattern::Trigger},
    {"", "HPR", CameraPattern::Trigger},
    {"", "HTS", CameraPattern::Trigger},
    {"", "HTU", CameraPattern::Trigger},
    {"", "HTD", CameraPattern::Trigger},
    {"", "HZS", CameraPattern::Trigger},
    {"", "HZW", CameraPattern::Trigger},
    {"", "HZT", CameraPattern::Trigger},
    {"", "HFS", CameraPattern::Trigger},
    {"", "HFN", CameraPattern::Trigger},
    {"", "HFF", CameraPattern::Trigger},
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
inline constexpr CameraCommandEntry kSceneCommand = {"QSF", "XSF", CameraPattern::Scene};

static_assert(kSceneCommand.pattern == CameraPattern::Scene, "kSceneCommand: Pattern 3 Scene");
static_assert(
    kSceneCommand.control[0] == 'X' && kSceneCommand.control[1] == 'S' &&
        kSceneCommand.control[2] == 'F' && kSceneCommand.control[3] == '\0',
    "kSceneCommand: control must be XSF");

/** Pattern 4: Dxx:n */
inline constexpr CameraCommandEntry kMonitorPrefix = {"", "D", CameraPattern::Monitor};

/** Pattern 8: Hxx */
inline constexpr CameraCommandEntry kLensHPrefix = {"", "H", CameraPattern::LensH};

} // namespace ccam::catalog
