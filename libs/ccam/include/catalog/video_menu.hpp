/**
 * @file video_menu.hpp
 * @brief Video menu: VideoMenuItem и kVideoMenu из одного списка.
 *
 * ConvertibleProtocol.pdf v3.05, стр. 14–15. QSA:nn / OSA:nn.
 * X(name, subcode)
 */

#pragma once

#include <cstddef>
#include <cstdint>

#define CCAM_FOR_EACH_VIDEO_ITEM(X)                                                                \
    X(GammaKneeVideo, 0x00)                                                                        \
    X(MGammaDrsOff, 0x01)                                                                          \
    X(MGammaDrsOn, 0x02)                                                                           \
    X(RGammaDrsOff, 0x03)                                                                          \
    X(RGammaDrsOn, 0x04)                                                                           \
    X(BGammaDrsOff, 0x05)                                                                          \
    X(BGammaDrsOn, 0x06)                                                                           \
    X(MBlackGamma, 0x07)                                                                           \
    X(RBlackGamma, 0x08)                                                                           \
    X(BBlackGamma, 0x09)                                                                           \
    X(GammaSw, 0x0A)                                                                               \
    X(BlackGammaSw, 0x0B)                                                                          \
    X(EffectDepth, 0x0C)                                                                           \
    X(DrsSw, 0x0D)                                                                                 \
    X(CineGammaSelect, 0x0E)                                                                       \
    X(BlackStretchLevel, 0x0F)                                                                     \
    X(DynamicLevel, 0x10)                                                                          \
    X(FlareSw, 0x11)                                                                               \
    X(RKneeSlope, 0x21)                                                                            \
    X(MKneeSlope, 0x25)                                                                            \
    X(RKneeSlopeVideo, 0x26)                                                                       \
    X(BKneeSlopeVideo, 0x27)                                                                       \
    X(AKneePoint, 0x28)                                                                            \
    X(AKneeLevel, 0x29)                                                                            \
    X(MWhiteClipLevel, 0x2A)                                                                       \
    X(RWhiteClipLevel, 0x2B)                                                                       \
    X(BWhiteClipLevel, 0x2C)                                                                       \
    X(KneeSw, 0x2D)                                                                                \
    X(WhiteClip, 0x2E)                                                                             \
    X(HighColor, 0x2F)                                                                             \
    X(TotalDtlLevel, 0x30)                                                                         \
    X(HDtlLevel, 0x31)                                                                             \
    X(PeakFrequency, 0x34)                                                                         \
    X(KneeAperture, 0x35)                                                                          \
    X(KneeApeLevel, 0x36)                                                                          \
    X(DetailPlus, 0x38)                                                                            \
    X(DetailMinus, 0x39)                                                                           \
    X(DetailClip, 0x3A)                                                                            \
    X(DetailSource, 0x3B)                                                                          \
    X(SkinToneDetailHd, 0x40)                                                                      \
    X(ModeAtSuperGain, 0x60)                                                                       \
    X(TotalGainAtSuperGain, 0x61)                                                                  \
    X(ScanReverse, 0x70)                                                                           \
    X(FrameRateRangeVariable, 0x71)                                                                \
    X(FrameRateTable, 0x81)                                                                        \
    X(ColorBarType, 0x84)                                                                          \
    X(MarkerPosition, 0x86)                                                                        \
    X(Format, 0x87)                                                                                \
    X(Status, 0x88)                                                                                \
    X(MenuOnBar, 0x89)                                                                             \
    X(MenuSel, 0x8A)                                                                               \
    X(ShutterMode, 0x90)                                                                           \
    X(ShutterSpeed, 0x91)                                                                          \
    X(GenlockInput, 0xA0)                                                                          \
    X(TotalDtlLevelHigh, 0xB1)                                                                     \
    X(BlackShadingCorrectDig, 0xC0)

#define CCAM_VIDEO_ENUM(name, code) name,
#define CCAM_VIDEO_ROW(name, code) code,

namespace ccam::catalog {

enum class VideoMenuItem : uint8_t {
    CCAM_FOR_EACH_VIDEO_ITEM(CCAM_VIDEO_ENUM)
    Count
};

inline constexpr uint8_t kVideoMenu[] = {
    CCAM_FOR_EACH_VIDEO_ITEM(CCAM_VIDEO_ROW)
};

inline constexpr size_t kVideoMenuCount = sizeof(kVideoMenu) / sizeof(kVideoMenu[0]);

static_assert(
    kVideoMenuCount == static_cast<size_t>(VideoMenuItem::Count),
    "kVideoMenu vs VideoMenuItem");

} // namespace ccam::catalog

#undef CCAM_VIDEO_ENUM
#undef CCAM_VIDEO_ROW
#undef CCAM_FOR_EACH_VIDEO_ITEM
