/**
 * @file catalog_helpers.hpp
 * @brief Доступ к каталогу по enum (ConvertibleProtocol.pdf v3.05).
 */

#pragma once

#include "catalog/camera_commands.hpp"
#include "catalog/command_ids.hpp"
#include "catalog/menu_catalog.hpp"
#include "catalog/pt_commands.hpp"

namespace ccam::catalog {

/** Банк subcode-меню (Pattern 5/7/9–18). */
enum class MenuBank : uint8_t {
    Osd,
    Video,
    DcBoard,
    ExtendedG,
    ExtendedI,
    ExtendedJ,
};

inline const char* menuSetPrefix(MenuBank bank)
{
    switch (bank) {
    case MenuBank::Osd:
        return "OSD";
    case MenuBank::Video:
        return "OSA";
    case MenuBank::DcBoard:
        return "OSE";
    case MenuBank::ExtendedG:
        return "OSG";
    case MenuBank::ExtendedI:
        return "OSI";
    case MenuBank::ExtendedJ:
        return "OSJ";
    default:
        return nullptr;
    }
}

inline const char* menuQueryPrefix(MenuBank bank)
{
    switch (bank) {
    case MenuBank::Osd:
        return "QSD";
    case MenuBank::Video:
        return "QSA";
    case MenuBank::DcBoard:
        return "QSE";
    case MenuBank::ExtendedG:
        return "QSG";
    case MenuBank::ExtendedI:
        return "QSI";
    case MenuBank::ExtendedJ:
        return "QSJ";
    default:
        return nullptr;
    }
}

inline MenuBank menuBankFor(OsdItem)
{
    return MenuBank::Osd;
}

inline MenuBank menuBankFor(VideoMenuItem)
{
    return MenuBank::Video;
}

inline MenuBank menuBankFor(ExtendedJItem)
{
    return MenuBank::ExtendedJ;
}

/** Control-команда камеры Oxx (3 символа) из каталога. */
inline constexpr const char* cameraControl(CameraCmd cmd)
{
    return getCameraCommand(cmd).control;
}

/** Query-команда камеры Qxx (3 символа) из каталога. */
inline constexpr const char* cameraQuery(CameraCmd cmd)
{
    return getCameraCommand(cmd).query;
}

/** P/T control без префикса '#' (PTS, P, APC, …). */
inline constexpr const char* ptControl(PtCmd cmd)
{
    const char* raw = getPtCommand(cmd).control;
    if (raw != nullptr && raw[0] == '#') {
        return raw + 1;
    }
    return raw;
}

} // namespace ccam::catalog
