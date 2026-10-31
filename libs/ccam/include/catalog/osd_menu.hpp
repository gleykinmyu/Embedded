/**
 * @file osd_menu.hpp
 * @brief User OSD: один список на OsdItem, subcode и поля параметра.
 *
 * ConvertibleProtocol.pdf v3.05, стр. 6–14. QSD:nn / OSD:nn.
 *
 * X(name, subcode, key, group, title, note, KIND, ...)
 *   LIN    min, max, bias, unit
 *   END    min, max, lo, hi
 *   CHOICE имя шкалы из этого же файла (kOsdGamma, kOsdOffOn, …)
 *   AGC    kOsdAgcStudio / kOsdAgcHe, выбор по модели в osd_params.cpp
 *
 * Здесь же enum, subcode и шкалы CHOICE.
 * osd_params.cpp только разворачивает строки в OsdParam и ходит на шину.
 */

#pragma once

#include "devices/cam_param_ranges.hpp"
#include "protocol/choice_lists.hpp"

#include <cstddef>
#include <cstdint>

#define CCAM_FOR_EACH_OSD_ITEM(X)                                                                  \
    X(Gamma, 0x00, "gamma", "Picture", "Gamma", nullptr, CHOICE, ::ccam::catalog::kOsdGamma)                           \
    X(KneePoint, 0x08, "knee", "Picture", "Knee Point",                                            \
        "PDF control≠query: UI/QSD FF=Dynamic, 00h…0Ah=88%…98%; OSD set 00 / 01h…0Bh", CHOICE,     \
        ::ccam::catalog::kOsdKnee)                                                                 \
    X(WhiteClip, 0x09, "wclip", "Picture", "White Clip", "PDF: 00h=95% … 0Fh=110%", LIN, 0x00,     \
        0x0F, 95, "%")                                                                             \
    X(HDtlLevelH, 0x0A, "hdtl_h", "Detail", "H DTL Level H", nullptr, LIN, 0x01, 0x3F, 0, nullptr) \
    X(HdHDtlLevelH, 0x0B, "hd_hdtl_h", "Detail", "HD H DTL Level H", nullptr, LIN, 0x01, 0x3F, 0,  \
        nullptr)                                                                                   \
    X(VDtlLevelH, 0x0E, "vdtl_h", "Detail", "V DTL Level H", nullptr, LIN, 0x01, 0x1F, 0, nullptr) \
    X(HdVDtlLevelH, 0x0F, "hd_vdtl_h", "Detail", "HD V DTL Level H", nullptr, LIN, 0x01, 0x1F, 0,  \
        nullptr)                                                                                   \
    X(HDtlLevelL, 0x12, "hdtl_l", "Detail", "H DTL Level L", nullptr, LIN, 0x00, 0x3E, 0, nullptr) \
    X(HdHDtlLevelL, 0x13, "hd_hdtl_l", "Detail", "HD H DTL Level L", nullptr, LIN, 0x00, 0x3E, 0,  \
        nullptr)                                                                                   \
    X(VDtlLevelL, 0x16, "vdtl_l", "Detail", "V DTL Level L", "PDF HE100: 00h=-7, 07h=0, 0Eh=+7",   \
        LIN, 0x00, 0x0E, -7, nullptr)                                                              \
    X(HdVDtlLevelL, 0x17, "hd_vdtl_l", "Detail", "HD V DTL Level L", nullptr, LIN, 0x00, 0x1E, 0,  \
        nullptr)                                                                                   \
    X(DetailBand, 0x1E, "dband", "Detail", "Detail Band", nullptr, LIN, 0x01, 0x05, 0, nullptr)    \
    X(HdDetailBand, 0x1F, "hd_dband", "Detail", "HD Detail Band",                                  \
        "HC1500/1800 в PDF ещё 00h…3Ch", LIN, 0x01, 0x05, 0, nullptr)                              \
    X(NoiseSuppressCrisp, 0x22, "ncrisp", "Detail", "Noise Suppress/Crisp", "UB300 в PDF до 63",   \
        LIN, 0x00, 0x1F, 0, nullptr)                                                               \
    X(HdNoiseSuppressCrisp, 0x23, "hd_ncrisp", "Detail", "HD Noise Suppress/Crisp",                \
        "PDF: шкала HE870 0…10", LIN, 0x00, 0x0A, 0, nullptr)                                      \
    X(LevelDependent, 0x26, "lvdep", "Detail", "Level Dependent",                                  \
        "PDF: 00h=0% … 0Fh=15%. HC3800 до 30%", LIN, 0x00, 0x0F, 0, "%")                           \
    X(HdLevelDependent, 0x27, "hd_lvdep", "Detail", "HD Level Dependent",                          \
        "PDF: шкала HE870 0…25%", LIN, 0x00, 0x19, 0, "%")                                         \
    X(ChromaDetail, 0x2A, "cdtl", "Detail", "Chroma Detail", nullptr, LIN, 0x00, 0x0F, 0, nullptr) \
    X(HdChromaDetail, 0x2B, "hd_cdtl", "Detail", "HD Chroma Detail", nullptr, LIN, 0x00, 0x0F, 0,  \
        nullptr)                                                                                   \
    X(HdDarkDetail, 0x2D, "hd_dark", "Detail", "HD Dark Detail", "HC3800 в PDF до 7", LIN, 0x00,   \
        0x05, 0, nullptr)                                                                          \
    X(DarkDetail, 0x2E, "dark", "Detail", "Dark Detail", nullptr, LIN, 0x00, 0x05, 0, nullptr)     \
    X(MatrixRG, 0x2F, "mx_rg", "Matrix", "Matrix R-G", "PDF: 00h=-31, 1Fh=0, 3Eh=+31", LIN, 0x00, \
        0x3E, -31, nullptr)                                                                        \
    X(MatrixRB, 0x30, "mx_rb", "Matrix", "Matrix R-B", nullptr, LIN, 0x00, 0x3E, -31, nullptr)    \
    X(MatrixGR, 0x31, "mx_gr", "Matrix", "Matrix G-R", nullptr, LIN, 0x00, 0x3E, -31, nullptr)    \
    X(MatrixGB, 0x32, "mx_gb", "Matrix", "Matrix G-B", nullptr, LIN, 0x00, 0x3E, -31, nullptr)    \
    X(MatrixBR, 0x33, "mx_br", "Matrix", "Matrix B-R", nullptr, LIN, 0x00, 0x3E, -31, nullptr)    \
    X(MatrixBG, 0x34, "mx_bg", "Matrix", "Matrix B-G", nullptr, LIN, 0x00, 0x3E, -31, nullptr)    \
    X(FlareR, 0x35, "fl_r", "Flare", "Flare R", "PDF: 00h=0 … 64h=100. AK-HC — другая шкала", LIN, \
        0x00, 0x64, 0, nullptr)                                                                    \
    X(FlareG, 0x36, "fl_g", "Flare", "Flare G", nullptr, LIN, 0x00, 0x64, 0, nullptr)              \
    X(FlareB, 0x37, "fl_b", "Flare", "Flare B", nullptr, LIN, 0x00, 0x64, 0, nullptr)              \
    X(CleanDnr, 0x3A, "cdnr", "Process", "Clean DNR", "UB300 в PDF: Off/On/On", CHOICE,            \
        ::ccam::catalog::kOsdOffLowHigh)                                                           \
    X(HdCleanDnr, 0x3B, "hd_cdnr", "Process", "HD Clean DNR", nullptr, CHOICE, ::ccam::catalog::kOsdOffLowHigh)        \
    X(Lpf2d, 0x3F, "lpf2d", "Process", "2D LPF", nullptr, CHOICE, ::ccam::catalog::kOsdOffLowHigh)                     \
    X(CornerDetail, 0x43, "corner", "Process", "Corner Detail", nullptr, CHOICE, ::ccam::catalog::kOsdOffOn)           \
    X(PrecisionDetail, 0x44, "prec", "Process", "Precision/Slim Detail", "HC1500 в PDF: Off/On/On", \
        CHOICE, ::ccam::catalog::kOsdOffLowHigh)                                                                       \
    X(HdPrecisionDetail, 0x45, "hd_prec", "Process", "HD Precision/Slim Detail",                   \
        "PDF: шкала HE870", CHOICE, ::ccam::catalog::kOsdOffLowHigh)                                                   \
    X(BlackStretch, 0x46, "bstr", "Process", "Black Stretch", nullptr, CHOICE, ::ccam::catalog::kOsdOffOn)             \
    X(AirisLevel, 0x48, "airis", "Process", "A.IRIS Level",                                        \
        "PDF: 00h=0 … 64h=100. HE100 — offset -4…+1", LIN, 0x00, 0x64, 0, nullptr)                \
    X(HighLightChroma, 0x49, "hlch", "Process", "High Light Chroma",                               \
        "PDF: колонки QSD нет, Read шлёт QSD:49", CHOICE, ::ccam::catalog::kOsdOffLowHigh)                             \
    X(FleshNoiseSuppress, 0x4B, "fns", "Process", "Flesh Noise Suppress", nullptr, CHOICE,         \
        ::ccam::catalog::kOsdLowMidHigh)                                                           \
    X(HdFleshNoiseSuppress, 0x4C, "hd_fns", "Process", "HD Flesh Noise Suppress", nullptr, CHOICE, \
        ::ccam::catalog::kOsdOffLowHigh)                                                           \
    X(IrisFollow, 0x4F, "ifollow", "Process", "Iris Follow",                                       \
        "шаг PDF не называет: 00 Close … FF Open", END, 0x00, 0xFF, "Close", "Open")              \
    X(ContrastGamma, 0x50, "contr", "Process", "Contrast (Gamma)", nullptr, CHOICE, ::ccam::catalog::kOsdLowMidHigh)   \
    X(FleshTone, 0x52, "ftone", "Process", "Flesh Tone", "PDF: 00h=-3 … 03h=0 … 06h=+3", LIN,     \
        0x00, 0x06, -3, nullptr)                                                                   \
    X(DetailSelect, 0x54, "dsel", "Process", "Detail Select", nullptr, CHOICE, ::ccam::catalog::kOsdDetailSel)         \
    X(NoiseSuppress, 0x55, "nsup", "Process", "Noise Suppress", nullptr, CHOICE, ::ccam::catalog::kOsdOffLowHigh)      \
    X(FleshNoseSuppress, 0x56, "fnose", "Process", "Flesh Nose Suppress",                          \
        "в PDF так и написано: NOSE", CHOICE, ::ccam::catalog::kOsdLowMidHigh)                                         \
    X(ZebraIndicator, 0x60, "zebra", "Monitor", "Zebra", nullptr, CHOICE, ::ccam::catalog::kOsdOffOn)                  \
    X(Zebra1Level, 0x61, "zeb1", "Monitor", "Zebra 1 Level", "PDF: 00h=70% … 27h=109%", LIN, 0x00, \
        0x27, 70, "%")                                                                             \
    X(Zebra2Level, 0x62, "zeb2", "Monitor", "Zebra 2 Level", "PDF: 01h=71% … 28h=110%", LIN, 0x01, \
        0x28, 70, "%")                                                                             \
    X(SafetyZone, 0x63, "szone", "Monitor", "Safety Zone", nullptr, CHOICE, ::ccam::catalog::kOsdZone)                 \
    X(EvfOutput, 0x64, "evf", "Monitor", "EVF Output", nullptr, CHOICE, ::ccam::catalog::kOsdEvf)                      \
    X(OutputSelect, 0x65, "outsel", "Monitor", "Output Select", nullptr, CHOICE, ::ccam::catalog::kOsdOut)             \
    X(ChargeTime, 0x68, "charge", "System", "Charge Time", nullptr, CHOICE, ::ccam::catalog::kOsdCharge)               \
    X(AgcMax, 0x69, "agcmax", "System", "AGC Max", nullptr, AGC)                                   \
    X(AspectRatio, 0x70, "aspect", "System", "Aspect Ratio", nullptr, CHOICE, ::ccam::catalog::kOsdAspect)             \
    X(Fan, 0x71, "fan", "System", "Fan", nullptr, CHOICE, ::ccam::catalog::kOsdFan)                                    \
    X(AtwSpeed, 0x72, "atw", "System", "ATW Speed", nullptr, CHOICE, ::ccam::catalog::kOsdAtw)                         \
    X(ColorMatrixBMgGain, 0x80, "cm_bmg_g", "Color", "Color Matrix B_Mg Gain",                     \
        "PDF: 01h=-127, 80h=0, FFh=+127", LIN, 0x01, 0xFF, -128, nullptr)                          \
    X(ColorMatrixBMgPhase, 0x81, "cm_bmg_p", "Color", "Color Matrix B_Mg Phase", nullptr, LIN,     \
        0x01, 0xFF, -128, nullptr)                                                                 \
    X(ColorMatrixMgGain, 0x82, "cm_mg_g", "Color", "Color Matrix Mg Gain", nullptr, LIN, 0x01,     \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixMgPhase, 0x83, "cm_mg_p", "Color", "Color Matrix Mg Phase", nullptr, LIN, 0x01,   \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixMgRGain, 0x84, "cm_mgr_g", "Color", "Color Matrix Mg_R Gain", nullptr, LIN, 0x01, \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixMgRPhase, 0x85, "cm_mgr_p", "Color", "Color Matrix Mg_R Phase", nullptr, LIN,     \
        0x01, 0xFF, -128, nullptr)                                                                 \
    X(ColorMatrixRGain, 0x86, "cm_r_g", "Color", "Color Matrix R Gain", nullptr, LIN, 0x01, 0xFF,  \
        -128, nullptr)                                                                             \
    X(ColorMatrixRPhase, 0x87, "cm_r_p", "Color", "Color Matrix R Phase", nullptr, LIN, 0x01, 0xFF, \
        -128, nullptr)                                                                             \
    X(ColorMatrixRYlGain, 0x88, "cm_ryl_g", "Color", "Color Matrix R_Yl Gain", nullptr, LIN, 0x01, \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixRYlPhase, 0x89, "cm_ryl_p", "Color", "Color Matrix R_Yl Phase", nullptr, LIN,     \
        0x01, 0xFF, -128, nullptr)                                                                 \
    X(ColorMatrixYlGain, 0x8A, "cm_yl_g", "Color", "Color Matrix Yl Gain", nullptr, LIN, 0x01,     \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixYlPhase, 0x8B, "cm_yl_p", "Color", "Color Matrix Yl Phase", nullptr, LIN, 0x01,   \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixYlGGain, 0x8C, "cm_ylg_g", "Color", "Color Matrix Yl_G Gain", nullptr, LIN, 0x01, \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixYlGPhase, 0x8D, "cm_ylg_p", "Color", "Color Matrix Yl_G Phase", nullptr, LIN,     \
        0x01, 0xFF, -128, nullptr)                                                                 \
    X(ColorMatrixGGain, 0x8E, "cm_g_g", "Color", "Color Matrix G Gain", nullptr, LIN, 0x01, 0xFF,  \
        -128, nullptr)                                                                             \
    X(ColorMatrixGPhase, 0x8F, "cm_g_p", "Color", "Color Matrix G Phase", nullptr, LIN, 0x01, 0xFF, \
        -128, nullptr)                                                                             \
    X(ColorMatrixGCyGain, 0x90, "cm_gcy_g", "Color", "Color Matrix G_Cy Gain", nullptr, LIN, 0x01, \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixGCyPhase, 0x91, "cm_gcy_p", "Color", "Color Matrix G_Cy Phase", nullptr, LIN,     \
        0x01, 0xFF, -128, nullptr)                                                                 \
    X(ColorMatrixCyGain, 0x92, "cm_cy_g", "Color", "Color Matrix Cy Gain", nullptr, LIN, 0x01,     \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixCyPhase, 0x93, "cm_cy_p", "Color", "Color Matrix Cy Phase", nullptr, LIN, 0x01,   \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixCyBGain, 0x94, "cm_cyb_g", "Color", "Color Matrix Cy_B Gain", nullptr, LIN, 0x01, \
        0xFF, -128, nullptr)                                                                       \
    X(ColorMatrixCyBPhase, 0x95, "cm_cyb_p", "Color", "Color Matrix Cy_B Phase", nullptr, LIN,     \
        0x01, 0xFF, -128, nullptr)                                                                 \
    X(ColorMatrixBGain, 0x96, "cm_b_g", "Color", "Color Matrix B Gain", nullptr, LIN, 0x01, 0xFF,  \
        -128, nullptr)                                                                             \
    X(ColorMatrixBPhase, 0x97, "cm_b_p", "Color", "Color Matrix B Phase", nullptr, LIN, 0x01, 0xFF, \
        -128, nullptr)

#define CCAM_OSD_ENUM(name, code, ...) name,
#define CCAM_OSD_ROW(name, code, ...) code,

namespace ccam::catalog {

enum class OsdItem : uint8_t {
    CCAM_FOR_EACH_OSD_ITEM(CCAM_OSD_ENUM)
    Count
};

inline constexpr uint8_t kOsdMenu[] = {
    CCAM_FOR_EACH_OSD_ITEM(CCAM_OSD_ROW)
};

inline constexpr size_t kOsdMenuCount = sizeof(kOsdMenu) / sizeof(kOsdMenu[0]);

static_assert(kOsdMenuCount == static_cast<size_t>(OsdItem::Count), "kOsdMenu vs OsdItem");

/** Шкалы пунктов CHOICE. Имена — в списке выше. */
inline constexpr ccam::devices::CamChoice kOsdOffOn[] = {CCAM_FOR_EACH_SWITCH(CCAM_CHOICE_HEX2)};
inline constexpr ccam::devices::CamChoice kOsdOffLowHigh[] = {
    {"00", "Off"}, {"01", "Low"}, {"02", "High"},
};
inline constexpr ccam::devices::CamChoice kOsdLowMidHigh[] = {
    {"00", "Low"}, {"01", "Mid"}, {"02", "High"},
};
inline constexpr ccam::devices::CamChoice kOsdGamma[] = {CCAM_FOR_EACH_GAMMA(CCAM_HEX_CHOICE)};
/** Knee Point — коды QSD/UI (control OSD = Dynamic→00h, % → query+1). */
inline constexpr ccam::devices::CamChoice kOsdKnee[] = {
    {"FF", "Dynamic"},
    {"00", "88%"},
    {"01", "89%"},
    {"02", "90%"},
    {"03", "91%"},
    {"04", "92%"},
    {"05", "93%"},
    {"06", "94%"},
    {"07", "95%"},
    {"08", "96%"},
    {"09", "97%"},
    {"0A", "98%"},
};
inline constexpr ccam::devices::CamChoice kOsdDetailSel[] = {{"00", "Normal"}, {"01", "Super DTL"}};
inline constexpr ccam::devices::CamChoice kOsdZone[] = {
    {"01", "1"}, {"02", "2"}, {"03", "3"}, {"04", "4"}, {"05", "5"}, {"06", "Off"},
};
inline constexpr ccam::devices::CamChoice kOsdEvf[] = {{"00", "Y"}, {"01", "VBS"}};
inline constexpr ccam::devices::CamChoice kOsdOut[] = {{"00", "RGB"}, {"01", "YPbPr"}, {"02", "Y/C"}};
inline constexpr ccam::devices::CamChoice kOsdCharge[] = {
    {"00", "2 s"},
    {"01", "1 s"},
    {"02", "1/2 s"},
    {"03", "1/4 s NTSC / 1/3 s PAL"},
    {"04", "1/8 s NTSC / 1/6 s PAL"},
    {"05", "1/15 s NTSC / 1/12 s PAL"},
    {"06", "1/30 s NTSC / 1/25 s PAL"},
    {"07", "Off"},
    {"08", "Auto"},
};
inline constexpr ccam::devices::CamChoice kOsdAgcStudio[] = {CCAM_FOR_EACH_AGC_STUDIO(CCAM_HEX_CHOICE)};
inline constexpr ccam::devices::CamChoice kOsdAgcHe[] = {CCAM_FOR_EACH_AGC_HE(CCAM_HEX_CHOICE)};
inline constexpr ccam::devices::CamChoice kOsdAspect[] = {{"00", "16:9"}, {"01", "4:3"}};
inline constexpr ccam::devices::CamChoice kOsdFan[] = {{"00", "Off"}, {"01", "On"}, {"02", "Auto"}};
inline constexpr ccam::devices::CamChoice kOsdAtw[] = {
    {"00", "Slow 2"}, {"01", "Slow 1"}, {"02", "Middle"}, {"03", "Fast 1"}, {"04", "Fast 2"},
};

} // namespace ccam::catalog

#undef CCAM_OSD_ENUM
#undef CCAM_OSD_ROW
