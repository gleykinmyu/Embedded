/**
 * @file camera_commands.hpp
 * @brief Camera Control: CameraCmd и kCameraCommands из одного списка.
 *
 * ConvertibleProtocol.pdf v3.05. Индекс enum = строка таблицы.
 * X(name, query, control, pattern)
 */

#pragma once

#include "catalog/protocol_patterns.hpp"

#include <cstddef>
#include <cstdint>

#define CCAM_FOR_EACH_CAMERA_CMD(X)                                                                \
    X(ModelNumber, "QID", "", Trigger)       /* только QID → OID */                                \
    X(SoftwareVersion, "QSV", "", Trigger)   /* только QSV → OSV */                                \
    X(AwcMode, "QAW", "OAW", SetO)                                                                 \
    X(Detail, "QDT", "ODT", SetO)                                                                  \
    X(HdDetail, "QHD", "OHD", SetO)                                                                \
    X(GainUp, "QGU", "OGU", SetO)                                                                  \
    X(Shutter, "QSH", "OSH", SetO)                                                                 \
    X(SynchroScan, "QMS", "OMS", SetO)                                                             \
    X(IrisAutoManual, "QRS", "ORS", SetO)                                                          \
    X(ManualIrisVolume, "QRV", "ORV", SetO)                                                        \
    X(LightPeakAvg, "QPA", "OPV", SetO)      /* Control OPV, query QPA → OPA */                    \
    X(LightArea, "QAR", "ORA", SetO)         /* Control ORA, query QAR → OAR */                    \
    X(NegPos, "QNP", "ONP", SetO)                                                                  \
    X(RPedestal, "QRD", "ORD", SetO)                                                               \
    X(BPedestal, "QBD", "OBD", SetO)                                                               \
    X(RGain, "QGR", "ORG", SetO)             /* Control ORG, query QGR → OGR */                    \
    X(BGain, "QGB", "OBG", SetO)             /* Control OBG, query QGB → OGB */                    \
    X(TPedestal, "QTD", "OTD", SetO)                                                               \
    X(HPhase, "QHP", "OHP", SetO)                                                                  \
    X(ScCoarse, "QSC", "OSC", SetO)                                                                \
    X(ScFine, "QSN", "OSN", SetO)                                                                  \
    X(ChromaLevel, "QCG", "OCG", SetO)                                                             \
    X(SceneFile, "QSF", "XSF", Scene)        /* Control XSF, query QSF → OSF */                    \
    X(FieldFrame, "QFF", "OFR", SetO)        /* Control OFR, query QFF → OFF */                    \
    X(AutoFocus, "QAF", "OAF", SetO)                                                               \
    X(DigitalGainUp, "QDG", "ODG", SetO)                                                           \
    X(DigitalExtender, "QDE", "ODE", SetO)                                                         \
    X(Dnr3d, "QDD", "ODD", SetO)                                                                   \
    X(ColorBar, "QBR", "DCB", SetO)          /* Control DCB, query QBR → OBR */                    \
    X(FilterNd, "QFT", "OFT", SetO)                                                                \
    X(RedTally, "QLR", "TLR", SetO)          /* Control TLR, query QLR → OLR */                    \
    X(GreenTally, "QLG", "TLG", SetO)        /* Control TLG, query QLG → OLG */                    \
    X(TPedestalVideo, "QTP", "OTP", SetO)                                                          \
    X(RGainVideo, "QRI", "ORI", SetO)                                                              \
    X(BGainVideo, "QBI", "OBI", SetO)                                                              \
    X(RPedestalVideo, "QRP", "ORP", SetO)                                                         \
    X(BPedestalVideo, "QBP", "OBP", SetO)                                                          \
    X(MenuOnOff, "QUS", "DUS", SetO)         /* Control DUS, query QUS → OUS */                    \
    X(BarSetup, "QCS", "DCS", SetO)          /* Control DCS, query QCS → OCS */                    \
    X(ZoomSpeedLens, "", "LZS", SetO)                                                              \
    X(FocusSpeedLens, "", "LFS", SetO)                                                             \
    X(SaveLensPreset, "", "LPS", SetO)                                                             \
    X(RecallLensPreset, "", "LPM", SetO)     /* не LPR */                                          \
    X(LensFocusStopNearFar, "", "HFS", Trigger) /* deprecated (=HFS); не путать с LFS */           \
    X(AwcAwbSet, "", "OWS", Trigger)                                                               \
    X(AbcAbbSet, "", "OAS", Trigger)                                                               \
    X(MenuSw, "", "DPG", SetO)                                                                     \
    X(ItemSw, "", "DIT", SetO)                                                                     \
    X(YesSw, "", "DUP", SetO)                                                                      \
    X(NoSw, "", "DDW", SetO)                                                                       \
    X(LeftSw, "", "DLT", SetO)                                                                     \
    X(RightSw, "", "DRT", SetO)                                                                    \
    X(PanStop, "", "HPS", Trigger)                                                                 \
    X(PanLeft, "", "HPL", Trigger)                                                                 \
    X(PanRight, "", "HPR", Trigger)                                                                \
    X(TiltStop, "", "HTS", Trigger)                                                                \
    X(TiltUp, "", "HTU", Trigger)                                                                  \
    X(TiltDown, "", "HTD", Trigger)                                                                \
    X(ZoomStop, "", "HZS", Trigger)                                                                \
    X(ZoomWide, "", "HZW", Trigger)                                                                \
    X(ZoomTele, "", "HZT", Trigger)                                                                \
    X(FocusStop, "", "HFS", Trigger)                                                               \
    X(FocusNear, "", "HFN", Trigger)                                                               \
    X(FocusFar, "", "HFF", Trigger)

#define CCAM_CAMERA_CMD_ENUM(name, query, control, pattern) name,
#define CCAM_CAMERA_CMD_ROW(name, query, control, pattern) {query, control, CameraPattern::pattern},

namespace ccam::catalog {

struct CameraCommandEntry {
    const char* query;
    const char* control;
    CameraPattern pattern;
};

enum class CameraCmd : uint8_t {
    CCAM_FOR_EACH_CAMERA_CMD(CCAM_CAMERA_CMD_ENUM)
    Count
};

inline constexpr CameraCommandEntry kCameraCommands[] = {
    CCAM_FOR_EACH_CAMERA_CMD(CCAM_CAMERA_CMD_ROW)
};

inline constexpr size_t kCameraCommandCount = sizeof(kCameraCommands) / sizeof(kCameraCommands[0]);

static_assert(
    kCameraCommandCount == static_cast<size_t>(CameraCmd::Count),
    "kCameraCommands must match CameraCmd enum");

inline constexpr const CameraCommandEntry& getCameraCommand(CameraCmd cmd)
{
    return kCameraCommands[static_cast<size_t>(cmd)];
}

/** Pattern 3: [STX]XSF:n[ETX] — строка CameraCmd::SceneFile. */
inline constexpr const CameraCommandEntry& kSceneCommand =
    kCameraCommands[static_cast<size_t>(CameraCmd::SceneFile)];

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

#undef CCAM_CAMERA_CMD_ENUM
#undef CCAM_CAMERA_CMD_ROW
#undef CCAM_FOR_EACH_CAMERA_CMD
