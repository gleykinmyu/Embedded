/**
 * @file video_menu.hpp
 * @brief Video menu QSA:nn / OSA:nn (ConvertibleProtocol.pdf v3.05, стр. 14–15).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/menu_entry.hpp"

#include <cstddef>

namespace ccam::catalog {

inline constexpr MenuSubcodeEntry kVideoMenu[] = {
    {0x00, "Gamma/Knee Video"},
    {0x01, "M Gamma@DRS OFF"},
    {0x02, "M Gamma@DRS ON"},
    {0x03, "R Gamma@DRS OFF"},
    {0x04, "R Gamma@DRS ON"},
    {0x05, "B Gamma@DRS OFF"},
    {0x06, "B Gamma@DRS ON"},
    {0x07, "M Black Gamma"},
    {0x08, "R Black Gamma"},
    {0x09, "B Black Gamma"},
    {0x0A, "Gamma SW"},
    {0x0B, "Black Gamma SW"},
    {0x0C, "Effect Depth"},
    {0x0D, "DRS SW"},
    {0x0E, "Cine Gamma Select"},
    {0x0F, "Black Stretch Level"},
    {0x10, "Dynamic Level"},
    {0x11, "Flare SW"},
    {0x21, "R Knee Slope"},
    {0x25, "M Knee Slope"},
    {0x26, "R Knee Slope (Video)"},
    {0x27, "B Knee Slope (Video)"},
    {0x28, "A.Knee Point"},
    {0x29, "A.Knee Level"},
    {0x2A, "M White Clip Level"},
    {0x2B, "R White Clip Level"},
    {0x2C, "B White Clip Level"},
    {0x2D, "Knee SW"},
    {0x2E, "White Clip"},
    {0x2F, "High Color"},
    {0x30, "Total DTL Level"},
    {0x31, "H DTL Level"},
    {0x34, "Peak Frequency"},
    {0x35, "Knee Aperture"},
    {0x36, "Knee Ape Level"},
    {0x38, "Detail (+)"},
    {0x39, "Detail (-)"},
    {0x3A, "Detail Clip"},
    {0x3B, "Detail Source"},
    {0x40, "Skin Tone Detail (HD)"},
    {0x60, "MODE @S.GAIN"},
    {0x61, "TOTAL GAIN @S.GAIN"},
    {0x70, "SCAN REVERSE"},
    {0x71, "FRAME RATE RANGE @VARIABLE FRAME"},
    {0x81, "Frame Rate Table"},
    {0x84, "Color Bar Type"},
    {0x86, "Marker Position"},
    {0x87, "FORMAT"},
    {0x88, "STATUS"},
    {0x89, "MENU ON BAR"},
    {0x8A, "MENU SEL"},
    {0x90, "SHUTTER MODE"},
    {0x91, "SHUTTER SPEED"},
    {0xA0, "GEN-LOCK INPUT"},
    {0xB1, "TOTAL DTL LEVEL HIGH"},
    {0xC0, "Black Shading Correct (DIG)"},
};

inline constexpr size_t kVideoMenuCount = sizeof(kVideoMenu) / sizeof(kVideoMenu[0]);

static_assert(
    kVideoMenuCount == static_cast<size_t>(VideoMenuItem::Count),
    "kVideoMenu vs VideoMenuItem");

} // namespace ccam::catalog
