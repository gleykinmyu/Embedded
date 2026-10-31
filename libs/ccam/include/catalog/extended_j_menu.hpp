/**
 * @file extended_j_menu.hpp
 * @brief Extended J: ExtendedJItem и kExtendedJMenu из одного списка.
 *
 * ConvertibleProtocol.pdf v3.05, стр. 24–27. QSJ:nn / OSJ:nn, HE/UE.
 * X(name, subcode)
 */

#pragma once

#include <cstddef>
#include <cstdint>

#define CCAM_FOR_EACH_EXTENDED_J(X)                                                                \
    X(Sdi3gHdrOutputSelect, 0x22)                                                                  \
    X(MoniOutFormat, 0x23)                                                                         \
    X(MoniOutHdrOutputSelect, 0x24)                                                                \
    X(HdmiOutFormat, 0x25)                                                                         \
    X(HdmiOutHdrOutputSelect, 0x26)                                                                \
    X(ColorBarTone, 0x27)                                                                          \
    X(SavePresetName, 0x35)                                                                        \
    X(PresetCounterBitmap, 0x3C)                                                                   \
    X(ZoomScale, 0x3D)                                                                             \
    X(OperationLockStatus, 0x40)                                                                   \
    X(PowerOnPosition, 0x45)                                                                       \
    X(PowerOnPresetNumber, 0x46)

#define CCAM_EXTJ_ENUM(name, code) name,
#define CCAM_EXTJ_ROW(name, code) code,

namespace ccam::catalog {

enum class ExtendedJItem : uint8_t {
    CCAM_FOR_EACH_EXTENDED_J(CCAM_EXTJ_ENUM)
    Count
};

inline constexpr uint8_t kExtendedJMenu[] = {
    CCAM_FOR_EACH_EXTENDED_J(CCAM_EXTJ_ROW)
};

inline constexpr size_t kExtendedJMenuCount =
    sizeof(kExtendedJMenu) / sizeof(kExtendedJMenu[0]);

static_assert(
    kExtendedJMenuCount == static_cast<size_t>(ExtendedJItem::Count),
    "kExtendedJMenu vs ExtendedJItem");

} // namespace ccam::catalog

#undef CCAM_EXTJ_ENUM
#undef CCAM_EXTJ_ROW
#undef CCAM_FOR_EACH_EXTENDED_J
