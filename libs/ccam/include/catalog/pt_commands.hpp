/**
 * @file pt_commands.hpp
 * @brief P/T: PtCmd и kPtCommands из одного списка.
 *
 * ConvertibleProtocol.pdf v3.05. control без '#'; префикс добавляет PtDeviceBase.
 * response пустой, если ответа нет.
 * X(name, control, response)
 */

#pragma once

#include <cstddef>
#include <cstdint>

#define CCAM_FOR_EACH_PT_CMD(X)                                                                    \
    X(Power, "O", "p")                                                                             \
    X(PanSpeed, "P", "pS")                                                                         \
    X(TiltSpeed, "T", "tS")                                                                        \
    X(ZoomSpeed, "Z", "zS")                                                                        \
    X(ZoomPositionSet, "AXZ", "axz")     /* только с Data; голый #AXZ не шлём */                  \
    X(FocusSpeed, "F", "fS")                                                                       \
    X(FocusPositionSet, "AXF", "axf")    /* только с Data */                                      \
    X(RollSpeed, "RO", "rO")                                                                       \
    X(IrisSpeed, "I", "iC")                                                                        \
    X(IrisPositionSet, "AXI", "axi")     /* только с Data */                                      \
    X(ExtenderAf, "D1", "d1")                                                                      \
    X(NdFilter, "D2", "d2")                                                                        \
    X(IrisAutoManual, "D3", "d3")                                                                  \
    X(Lamp, "D4", "d4")                                                                            \
    X(LampAlarm, "D5", "d5")                                                                       \
    X(OptionSwDayNight, "D6", "d6")                                                                \
    X(Defroster, "D7", "d7")                                                                       \
    X(Wiper, "D8", "d8")                                                                           \
    X(HeaterFan, "D9", "d9")                                                                       \
    X(Tally, "DA", "dA")                                                                           \
    X(LatestRecallPresetNo, "S", "s")                                                              \
    X(SavePreset, "M", "s")                                                                        \
    X(RecallPreset, "R", "s")                                                                      \
    X(DeletePreset, "C", "s")                                                                      \
    X(PresetDoneNotify, "", "q")                                                                   \
    X(PresetMode, "RT", "rt")                                                                      \
    X(LimitationSet, "L", "l")                                                                     \
    X(LandingMode, "N", "n")                                                                       \
    X(ZoomDaPosition, "GZ", "gz")                                                                  \
    X(FocusDaPosition, "GF", "gf")                                                                 \
    X(IrisDaPosition, "GI", "gi")                                                                  \
    X(TiltRange, "AGL", "aGL")                                                                     \
    X(SoftwareVersionQuery, "QSV", "qSV")                                                          \
    X(SoftwareVersionCtrl, "CSV", "")                                                              \
    X(FirmwareVersion, "V?", "")                                                                   \
    X(TallyEnable, "TAE", "tAE")                                                                   \
    X(InstallPosition, "INS", "iNS")                                                               \
    X(SpeedWithZoomPos, "SWZ", "sWZ")                                                              \
    X(PanTiltAbsolute, "APC", "aPC")                                                               \
    X(PanTiltAbsoluteSpeed, "APS", "aPS")                                                          \
    X(PanTiltRelative, "RPC", "rPC")                                                               \
    X(PanTiltRelativeSpeed, "RPS", "rPS")                                                          \
    X(LimitationControl, "LC", "lC")                                                               \
    X(PanTiltSpeed, "PTS", "pTS")                                                                  \
    X(WirelessControl, "WLC", "wLC")                                                               \
    X(ErrorStatus, "RER", "rER")                                                                   \
    X(LensPositionInfo, "LPI", "lPI")                                                              \
    X(LensPositionCtrl, "LPC", "lPC")                                                              \
    X(SmartPictureFlip, "SPF", "sPF")                                                              \
    X(FlipDetectAngle, "FDA", "fDA")                                                               \
    X(PinpPosition, "PD", "pD")                                                                    \
    X(CameraPinp, "CMP", "cMP")                                                                    \
    X(GuideLine, "GDL", "gDL")                                                                     \
    X(IrRemoteId, "RID", "rID")                                                                    \
    X(Resolution, "RZL", "rZL")                                                                    \
    X(ImageFreezePreset, "PRF", "pRF")                                                             \
    X(PresetSpeedTable, "PST", "pST")                                                              \
    X(StatusLamp, "LMP", "lMP")                                                                    \
    X(Fan, "FAN", "fAN")                                                                           \
    X(Fan2, "FA2", "fA2")                                                                          \
    X(WiperCtrl, "WIP", "wIP")                                                                     \
    X(Washer, "WAS", "wAS")                                                                        \
    X(FanStatus1, "FS1", "fS1")                                                                    \
    X(FanStatus2, "FS2", "fS2")                                                                    \
    X(HeaterStatus, "HS", "hS")                                                                    \
    X(DefrosterStatus, "DS", "dS")                                                                 \
    X(WasherPtPosition, "WPT", "wPT")                                                              \
    X(WasherPtReset, "WPR", "wPR")                                                                 \
    X(GetGainShutterNd, "PTG", "pTG")                                                              \
    X(GetPtzPosRaw, "PTV", "pTV")                                                                  \
    X(GetPtzPosDisplay, "PTD", "pTD")                                                              \
    X(TallyInformation, "TAA", "tAA")                                                              \
    X(PresetSpeedUnit, "UPVS", "uPVS")                                                             \
    X(Option, "OPT", "oPT")                                                                        \
    X(PresetEntryConfirm, "PE", "pE")

#define CCAM_PT_CMD_ENUM(name, control, response) name,
#define CCAM_PT_CMD_ROW(name, control, response) {control, response},

namespace ccam::catalog {

struct PtCommandEntry {
    const char* control;
    const char* response;
};

enum class PtCmd : uint8_t {
    CCAM_FOR_EACH_PT_CMD(CCAM_PT_CMD_ENUM)
    Count
};

inline constexpr PtCommandEntry kPtCommands[] = {
    CCAM_FOR_EACH_PT_CMD(CCAM_PT_CMD_ROW)
};

inline constexpr size_t kPtCommandCount = sizeof(kPtCommands) / sizeof(kPtCommands[0]);

static_assert(
    kPtCommandCount == static_cast<size_t>(PtCmd::Count),
    "kPtCommands must match PtCmd enum");

inline constexpr const PtCommandEntry& getPtCommand(PtCmd cmd)
{
    return kPtCommands[static_cast<size_t>(cmd)];
}

/** Control без '#'. */
inline constexpr const char* ptControl(PtCmd cmd)
{
    return getPtCommand(cmd).control;
}

/** Ожидаемый префикс ответа. */
inline constexpr const char* ptResponse(PtCmd cmd)
{
    return getPtCommand(cmd).response;
}

} // namespace ccam::catalog

#undef CCAM_PT_CMD_ENUM
#undef CCAM_PT_CMD_ROW
#undef CCAM_FOR_EACH_PT_CMD
