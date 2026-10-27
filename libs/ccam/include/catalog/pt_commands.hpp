/**
 * @file pt_commands.hpp
 * @brief Каталог команд P/T (ConvertibleProtocol.pdf v3.05).
 * control без '#'; префикс добавляет PtDeviceBase::buildCommand.
 * response — ожидаемый префикс ответа (пусто, если ответа нет / notify).
 */

#pragma once

#include "catalog/command_ids.hpp"

#include <cstddef>

namespace ccam::catalog {

struct PtCommandEntry {
    const char* control;
    const char* response;
};

/** Индекс = PtCmd. */
inline constexpr PtCommandEntry kPtCommands[] = {
    {"O", "p"},        // Power
    {"P", "pS"},       // Pan Speed
    {"T", "tS"},       // Tilt Speed
    {"Z", "zS"},       // Zoom Speed
    {"AXZ", "axz"},    // Zoom Position Set
    {"AYZ", "ayz"},    // Zoom Position Query
    {"F", "fS"},       // Focus Speed
    {"AXF", "axf"},    // Focus Position Set
    {"AYF", "ayf"},    // Focus Position Query
    {"RO", "rO"},      // Roll Speed
    {"I", "iC"},       // Iris Speed
    {"AXI", "axi"},    // Iris Position Set
    {"AYI", "ayi"},    // Iris Position Query
    {"D1", "d1"},      // Extender/AF
    {"D2", "d2"},      // ND Filter
    {"D3", "d3"},      // Iris Auto/Manual
    {"D4", "d4"},      // Lamp
    {"D5", "d5"},      // Lamp Alarm
    {"D6", "d6"},      // Option SW Day/Night
    {"D7", "d7"},      // Defroster
    {"D8", "d8"},      // Wiper
    {"D9", "d9"},      // Heater/Fan
    {"DA", "dA"},      // Tally
    {"S", "s"},        // Latest Recall Preset No.
    {"M", "s"},        // Save Preset
    {"R", "s"},        // Recall Preset
    {"C", "s"},        // Delete Preset
    {"", "q"},         // Preset Done Notify
    {"RT", "rt"},      // Preset Mode
    {"L", "l"},        // Limitation Set
    {"N", "n"},        // Landing Mode
    {"GZ", "gz"},      // Zoom D/A Position
    {"GF", "gf"},      // Focus D/A Position
    {"GI", "gi"},      // Iris D/A Position
    {"AGL", "aGL"},    // Tilt Range
    {"QSV", "qSV"},    // Software Version Query
    {"CSV", ""},       // Software Version Ctrl
    {"V?", ""},        // Firmware Version
    {"TAE", "tAE"},    // Tally Enable
    {"INS", "iNS"},    // Install Position
    {"SWZ", "sWZ"},    // Speed With Zoom POS
    {"APC", "aPC"},    // Pan/Tilt Absolute
    {"APS", "aPS"},    // Pan/Tilt Absolute+Speed
    {"RPC", "rPC"},    // Pan/Tilt Relative
    {"RPS", "rPS"},    // Pan/Tilt Relative+Speed
    {"LC", "lC"},      // Limitation Control
    {"PTS", "pTS"},    // Pan+Tilt Speed
    {"WLC", "wLC"},    // Wireless Control
    {"RER", "rER"},    // Error Status
    {"LPI", "lPI"},    // Lens Position Info
    {"LPC", "lPC"},    // Lens Position Ctrl
    {"SPF", "sPF"},    // Smart Picture Flip
    {"FDA", "fDA"},    // Flip Detect Angle
    {"PD", "pD"},      // PinP Position
    {"CMP", "cMP"},    // Camera/PinP
    {"GDL", "gDL"},    // Guide Line
    {"RID", "rID"},    // IR Remote ID
    {"RZL", "rZL"},    // Resolution
    {"PRF", "pRF"},    // Image Freeze Preset
    {"PST", "pST"},    // Preset Speed Table
    {"LMP", "lMP"},    // Status Lamp
    {"FAN", "fAN"},    // Fan
    {"FA2", "fA2"},    // Fan2
    {"WIP", "wIP"},    // Wiper Ctrl
    {"WAS", "wAS"},    // Washer
    {"FS1", "fS1"},    // Fan Status1
    {"FS2", "fS2"},    // Fan Status2
    {"HS", "hS"},      // Heater Status
    {"DS", "dS"},      // Defroster Status
    {"WPT", "wPT"},    // Washer PT Position
    {"WPR", "wPR"},    // Washer PT Reset
    {"PTG", "pTG"},    // Get Gain/Shutter/ND
    {"PTV", "pTV"},    // Get PTZ Pos (raw)
    {"PTD", "pTD"},    // Get PTZ Pos (display)
    {"TAA", "tAA"},    // Tally Information
    {"UPVS", "uPVS"},  // Preset Speed Unit
    {"OPT", "oPT"},    // Option
    {"PE", "pE"},      // Preset Entry Confirm
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
