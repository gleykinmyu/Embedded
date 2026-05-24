/**
 * @file osd_menu.hpp
 * @brief User Mode OSD QSD:nn / OSD:nn (ConvertibleProtocol.pdf v3.05, стр. 6–14).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/menu_entry.hpp"

#include <cstddef>

namespace ccam::catalog {

inline constexpr MenuSubcodeEntry kOsdMenu[] = {
    {0x00, "Gamma"},
    {0x08, "Knee Point"},
    {0x09, "White Clip"},
    {0x0A, "H DTL Level H"},
    {0x0B, "HD H DTL Level H"},
    {0x0E, "V DTL Level H"},
    {0x0F, "HD V DTL Level H"},
    {0x12, "H DTL Level L"},
    {0x13, "HD H DTL Level L"},
    {0x16, "V DTL Level L"},
    {0x17, "HD V DTL Level L"},
    {0x1E, "Detail Band"},
    {0x1F, "HD Detail Band"},
    {0x22, "Noise Suppress/Crisp"},
    {0x23, "HD Noise Suppress/Crisp"},
    {0x26, "Level Dependent"},
    {0x27, "HD Level Dependent"},
    {0x2A, "Chroma Detail"},
    {0x2B, "HD Chroma Detail"},
    {0x2D, "HD Dark Detail"},
    {0x2E, "Dark Detail"},
    {0x2F, "Matrix R-G"},
    {0x30, "Matrix R-B"},
    {0x31, "Matrix G-R"},
    {0x32, "Matrix G-B"},
    {0x33, "Matrix B-R"},
    {0x34, "Matrix B-G"},
    {0x35, "Flare R"},
    {0x36, "Flare G"},
    {0x37, "Flare B"},
    {0x3A, "Clean DNR"},
    {0x3B, "HD Clean DNR"},
    {0x3F, "2D LPF"},
    {0x43, "Corner Detail"},
    {0x44, "Precision Detail/Slim Detail"},
    {0x45, "HD Precision Detail/HD Slim Detail"},
    {0x46, "Black Stretch"},
    {0x48, "A.IRIS Level/IRIS Offset"},
    {0x49, "High Light Chroma"},
    {0x4B, "Flesh Noise Suppress"},
    {0x4C, "HD Flesh Noise Suppress"},
    {0x4F, "Iris Follow"},
    {0x50, "Contrast (Gamma)"},
    {0x52, "Flesh Tone"},
    {0x54, "Detail Select"},
    {0x55, "Noise Suppress"},
    {0x56, "Flesh Nose Suppress"},
    {0x60, "Zebra Indicator"},
    {0x61, "Zebra1 Level"},
    {0x62, "Zebra2 Level"},
    {0x63, "Safety Zone"},
    {0x64, "EVF Output"},
    {0x65, "Output Select"},
    {0x68, "Charge Time"},
    {0x69, "AGC Max"},
    {0x70, "Aspect Ratio"},
    {0x71, "Fan"},
    {0x72, "ATW Speed"},
    {0x80, "Color Matrix B_Mg Gain"},
    {0x81, "Color Matrix B_Mg Phase"},
    {0x82, "Color Matrix Mg Gain"},
    {0x83, "Color Matrix Mg Phase"},
    {0x84, "Color Matrix Mg_B Gain"},
    {0x85, "Color Matrix Mg_B Phase"},
    {0x86, "Color Matrix R Gain"},
    {0x87, "Color Matrix R Phase"},
    {0x88, "Color Matrix R_Yl Gain"},
    {0x89, "Color Matrix R_Yl Phase"},
    {0x8A, "Color Matrix Yl Gain"},
    {0x8B, "Color Matrix Yl Phase"},
    {0x8C, "Color Matrix Yl_G Gain"},
    {0x8D, "Color Matrix Yl_G Phase"},
    {0x8E, "Color Matrix G Gain"},
    {0x8F, "Color Matrix G Phase"},
    {0x90, "Color Matrix G_Cy Gain"},
    {0x91, "Color Matrix G_Cy Phase"},
    {0x92, "Color Matrix Cy Gain"},
    {0x93, "Color Matrix Cy Phase"},
    {0x94, "Color Matrix Cy_B Gain"},
    {0x95, "Color Matrix Cy_B Phase"},
    {0x96, "Color Matrix B Gain"},
    {0x97, "Color Matrix B Phase"},
};

inline constexpr size_t kOsdMenuCount = sizeof(kOsdMenu) / sizeof(kOsdMenu[0]);

static_assert(kOsdMenuCount == static_cast<size_t>(OsdItem::Count), "kOsdMenu vs OsdItem");

} // namespace ccam::catalog
