/**
 * @file pt_commands.hpp
 * @brief Каталог команд P/T Control Protocol (ConvertibleProtocol.pdf v3.05).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/protocol_patterns.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::catalog {

struct PtCommandEntry {
    const char* name;
    const char* control;
    const char* response;
    PtPattern pattern;
    const char* data_fmt;
};

/** Индекс = PtCmd. */
inline constexpr PtCommandEntry kPtCommands[] = {
    {"Power", "#O", "p", PtPattern::HashCmd, "dec1"},
    {"Pan Speed", "#P", "pS", PtPattern::HashCmd, "dec2"},
    {"Tilt Speed", "#T", "tS", PtPattern::HashCmd, "dec2"},
    {"Zoom Speed", "#Z", "zS", PtPattern::HashCmd, "dec2"},
    {"Zoom Position Set", "#AXZ", "axz", PtPattern::HashCmd, "hex3"},
    {"Zoom Position Query", "#AYZ", "axz", PtPattern::HashQuery, "hex3/ dec3"},
    {"Focus Speed", "#F", "fS", PtPattern::HashCmd, "dec2"},
    {"Focus Position Set", "#AXF", "axf", PtPattern::HashCmd, "hex3"},
    {"Focus Position Query", "#AYF", "axf", PtPattern::HashQuery, "hex3/ dec3"},
    {"Roll Speed", "#RO", "rO", PtPattern::HashCmd, "dec2"},
    {"Iris Speed", "#I", "iC", PtPattern::HashCmd, "dec2"},
    {"Iris Position Set", "#AXI", "axi", PtPattern::HashCmd, "hex3"},
    {"Iris Position Query", "#AYI", "axi", PtPattern::HashQuery, "hex3/ dec3"},
    {"Extender/AF", "#D1", "d1", PtPattern::HashCmd, "dec1"},
    {"ND Filter", "#D2", "d2", PtPattern::HashCmd, "dec1"},
    {"Iris Auto/Manual", "#D3", "d3", PtPattern::HashCmd, "dec1"},
    {"Lamp", "#D4", "d4", PtPattern::HashCmd, "dec1"},
    {"Lamp Alarm", "#D5", "d5", PtPattern::HashQuery, "dec1"},
    {"Option SW Day/Night", "#D6", "d6", PtPattern::HashCmd, "dec1"},
    {"Defroster", "#D7", "d7", PtPattern::HashQuery, "dec1"},
    {"Wiper", "#D8", "d8", PtPattern::HashQuery, "dec1"},
    {"Heater/Fan", "#D9", "d9", PtPattern::HashQuery, "dec1"},
    {"Tally", "#DA", "dA", PtPattern::HashCmd, "dec1"},
    {"Latest Recall Preset No.", "#S", "s", PtPattern::HashQuery, "dec2"},
    {"Save Preset", "#M", "s", PtPattern::HashCmd, "dec2"},
    {"Recall Preset", "#R", "s", PtPattern::HashCmd, "dec2"},
    {"Delete Preset", "#C", "s", PtPattern::HashCmd, "dec2"},
    {"Preset Done Notify", "", "q", PtPattern::HashQuery, "dec2"},
    {"Preset Mode", "#RT", "rt", PtPattern::HashCmd, "dec1"},
    {"Limitation Set", "#L", "l", PtPattern::HashCmd, "dec1"},
    {"Landing Mode", "#N", "n", PtPattern::HashQuery, "dec1"},
    {"Zoom D/A Position", "#GZ", "gz", PtPattern::HashQuery, "hex3"},
    {"Focus D/A Position", "#GF", "gf", PtPattern::HashQuery, "hex3"},
    {"Iris D/A Position", "#GI", "gi", PtPattern::HashQuery, "hex3+dec1"},
    {"Tilt Range", "#AGL", "aGL", PtPattern::HashCmd, "dec1"},
    {"Software Version Query", "#QSV", "qSV", PtPattern::HashExtended, "unit+ver"},
    {"Software Version Ctrl", "#CSV", "", PtPattern::HashExtended, "unit+ver"},
    {"Firmware Version", "#V?", "", PtPattern::HashQuery, "ascii"},
    {"Tally Enable", "#TAE", "tAE", PtPattern::HashCmd, "dec1"},
    {"Install Position", "#INS", "iNS", PtPattern::HashCmd, "dec1"},
    {"Speed With Zoom POS", "#SWZ", "sWZ", PtPattern::HashCmd, "dec1"},
    {"Pan/Tilt Absolute", "#APC", "aPC", PtPattern::HashExtended, "hex4+hex4"},
    {"Pan/Tilt Absolute+Speed", "#APS", "aPS", PtPattern::HashExtended, "hex4+hex4+hex2+hex1"},
    {"Pan/Tilt Relative", "#RPC", "rPC", PtPattern::HashExtended, "hex4+hex4"},
    {"Pan/Tilt Relative+Speed", "#RPS", "rPS", PtPattern::HashExtended, "hex4+hex4+hex2+hex1"},
    {"Limitation Control", "#LC", "lC", PtPattern::HashExtended, "dec1+dec1"},
    {"Pan+Tilt Speed", "#PTS", "pTS", PtPattern::HashExtended, "dec2+dec2"},
    {"Wireless Control", "#WLC", "wLC", PtPattern::HashCmd, "dec1"},
    {"Error Status", "#RER", "rER", PtPattern::HashQuery, "hex2"},
    {"Lens Position Info", "#LPI", "lPI", PtPattern::HashQuery, "hex3x3"},
    {"Lens Position Ctrl", "#LPC", "lPC", PtPattern::HashCmd, "dec1"},
    {"Smart Picture Flip", "#SPF", "sPF", PtPattern::HashCmd, "dec1"},
    {"Flip Detect Angle", "#FDA", "fDA", PtPattern::HashCmd, "hex2"},
    {"PinP Position", "#PD", "pD", PtPattern::HashCmd, "dec1"},
    {"Camera/PinP", "#CMP", "cMP", PtPattern::HashCmd, "dec1"},
    {"Guide Line", "#GDL", "gDL", PtPattern::HashCmd, "dec1"},
    {"IR Remote ID", "#RID", "rID", PtPattern::HashCmd, "dec1"},
    {"Resolution", "#RZL", "rZL", PtPattern::HashCmd, "dec1"},
    {"Image Freeze Preset", "#PRF", "pRF", PtPattern::HashCmd, "dec1"},
    {"Preset Speed Table", "#PST", "pST", PtPattern::HashCmd, "dec1"},
    {"Status Lamp", "#LMP", "lMP", PtPattern::HashCmd, "dec1"},
    {"Fan", "#FAN", "fAN", PtPattern::HashCmd, "dec1"},
    {"Fan2", "#FA2", "fA2", PtPattern::HashCmd, "dec1"},
    {"Wiper Ctrl", "#WIP", "wIP", PtPattern::HashCmd, "dec1"},
    {"Washer", "#WAS", "wAS", PtPattern::HashCmd, "dec1"},
    {"Fan Status1", "#FS1", "fS1", PtPattern::HashQuery, "dec1"},
    {"Fan Status2", "#FS2", "fS2", PtPattern::HashQuery, "dec1"},
    {"Heater Status", "#HS", "hS", PtPattern::HashQuery, "dec1"},
    {"Defroster Status", "#DS", "dS", PtPattern::HashQuery, "dec1"},
    {"Washer PT Position", "#WPT", "wPT", PtPattern::HashQuery, ""},
    {"Washer PT Reset", "#WPR", "wPR", PtPattern::HashQuery, ""},
    {"Get Gain/Shutter/ND", "#PTG", "pTG", PtPattern::HashQuery, "hex multi"},
    {"Get PTZ Pos (raw)", "#PTV", "pTV", PtPattern::HashQuery, "hex4x2+hex3x3"},
    {"Get PTZ Pos (display)", "#PTD", "pTD", PtPattern::HashQuery, "hex/dec mix"},
    {"Tally Information", "#TAA", "tAA", PtPattern::HashQuery, "dec x9"},
    {"Preset Speed Unit", "#UPVS", "uPVS", PtPattern::HashCmd, "hex3"},
    {"Option", "#OPT", "oPT", PtPattern::HashCmd, "dec1"},
    {"Preset Entry Confirm", "#PE", "pE", PtPattern::HashQuery, "hex multi"},
};

inline constexpr size_t kPtCommandCount = sizeof(kPtCommands) / sizeof(kPtCommands[0]);

static_assert(
    kPtCommandCount == static_cast<size_t>(PtCmd::Count),
    "kPtCommands must match PtCmd enum");

inline constexpr const PtCommandEntry& getPtCommand(PtCmd cmd)
{
    return kPtCommands[static_cast<size_t>(cmd)];
}

} // namespace ccam::catalog
