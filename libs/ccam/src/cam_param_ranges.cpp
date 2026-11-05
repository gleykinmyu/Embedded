#include "devices/cam_param_ranges.hpp"

#include "protocol/choice_lists.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#define CCAM_ASSERT_DEC(name, value, wire, label) static_assert(::ccam::choiceDec(wire) == (value), #name);
#define CCAM_ASSERT_HEX(value, wire, label) static_assert(::ccam::choiceHex(wire) == (value), wire);
#define CCAM_ASSERT_SW(name, value, dec, hex2, label)                                                  \
    static_assert(::ccam::choiceDec(dec) == (value), #name);                                           \
    static_assert(::ccam::choiceHex(hex2) == (value), #name " hex");

CCAM_FOR_EACH_STUDIO_AWC(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_HE_AWC(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_SCENE(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_UE_SCENE(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_STUDIO_SHUTTER(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_E650_SHUTTER(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_COLOR_BAR(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_ND(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_SC_COARSE(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_LIGHT_AREA(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_SWITCH(CCAM_ASSERT_SW)
CCAM_FOR_EACH_FIELD_FRAME(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_COLOR_BAR(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_IRIS_AUTO(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_PR_MODE(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_PR_SPEED(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_LIMITATION(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_LIMIT_AXIS(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_TILT_RANGE(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_FAN(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_WIPER(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_LANDING(CCAM_ASSERT_DEC)
CCAM_FOR_EACH_GAMMA(CCAM_ASSERT_HEX)
CCAM_FOR_EACH_AGC_STUDIO(CCAM_ASSERT_HEX)
CCAM_FOR_EACH_AGC_E650(CCAM_ASSERT_HEX)
CCAM_FOR_EACH_AGC_HE(CCAM_ASSERT_HEX)

#undef CCAM_ASSERT_DEC
#undef CCAM_ASSERT_HEX
#undef CCAM_ASSERT_SW

namespace ccam::devices {
namespace {

CamParamSpec choicesOf(const CamChoice* c, size_t n)
{
    CamParamSpec s{};
    s.kind = CamParamKind::Choices;
    s.choices = c;
    s.choice_count = n;
    return s;
}

CamParamSpec hexRangeOf(uint32_t lo, uint32_t hi, uint8_t digits)
{
    CamParamSpec s{};
    s.kind = CamParamKind::HexRange;
    s.min_u = lo;
    s.max_u = hi;
    s.hex_digits = digits;
    return s;
}

const CamChoice kStudioAwc[] = {CCAM_FOR_EACH_STUDIO_AWC(CCAM_CHOICE)};
const CamChoice kHeAwc[] = {CCAM_FOR_EACH_HE_AWC(CCAM_CHOICE)};
const CamChoice kStudioScene[] = {CCAM_FOR_EACH_SCENE(CCAM_CHOICE)};
const CamChoice kUeScene[] = {CCAM_FOR_EACH_UE_SCENE(CCAM_CHOICE)};

/** OSH полный PDF (E600 / fallback). */
const CamChoice kStudioShutter[] = {CCAM_FOR_EACH_STUDIO_SHUTTER(CCAM_CHOICE)};

/** AW-E650E UI: Off, 1/120…1/10000, Synchro, ELC. */
const CamChoice kE650Shutter[] = {CCAM_FOR_EACH_E650_SHUTTER(CCAM_CHOICE)};

/** OSD:00 — тот же список, что вкладка OSD. */
const CamChoice kStudioGamma[] = {CCAM_FOR_EACH_GAMMA(CCAM_HEX_CHOICE)};

const CamChoice kStudioAgcMax[] = {CCAM_FOR_EACH_AGC_STUDIO(CCAM_HEX_CHOICE)};

/** AW-E650E: 6 / 12 / 18 dB (без Off). */
const CamChoice kE650AgcMax[] = {CCAM_FOR_EACH_AGC_E650(CCAM_HEX_CHOICE)};

const CamChoice kStudioTotalDtl[] = {
    {"70", "-16"},
    {"78", "-8"},
    {"80", "0"},
    {"88", "+8"},
    {"90", "+16"},
    {"9F", "+31"},
};

const CamChoice kOnOff[] = {CCAM_FOR_EACH_SWITCH(CCAM_CHOICE_DEC)};

const CamChoice kColorBar[] = {CCAM_FOR_EACH_COLOR_BAR(CCAM_CHOICE)};

const CamChoice kHeAgcMax[] = {CCAM_FOR_EACH_AGC_HE(CCAM_HEX_CHOICE)};

CamParamCatalog makeStudio()
{
    CamParamCatalog c{};
    c.awc = choicesOf(kStudioAwc, sizeof(kStudioAwc) / sizeof(kStudioAwc[0]));
    c.scene = choicesOf(kStudioScene, sizeof(kStudioScene) / sizeof(kStudioScene[0]));
    c.shutter = choicesOf(kStudioShutter, sizeof(kStudioShutter) / sizeof(kStudioShutter[0]));
    // OMS Synchro-Scan (ConvertibleProtocol): E/MC 001h=50.24Hz…137h=15.63kHz;
    // N model 001h=60.34Hz…105h=15.75kHz. Для E650E берём E/MC.
    c.synchro = hexRangeOf(1, 0x137, 3);
    c.gamma = choicesOf(kStudioGamma, sizeof(kStudioGamma) / sizeof(kStudioGamma[0]));
    c.agc_max = choicesOf(kStudioAgcMax, sizeof(kStudioAgcMax) / sizeof(kStudioAgcMax[0]));
    c.total_dtl = choicesOf(kStudioTotalDtl, sizeof(kStudioTotalDtl) / sizeof(kStudioTotalDtl[0]));
    c.genlock = choicesOf(kOnOff, sizeof(kOnOff) / sizeof(kOnOff[0]));
    c.colorbar = choicesOf(kColorBar, sizeof(kColorBar) / sizeof(kColorBar[0]));
    return c;
}

/** AW-E650E: shutter/AGC как в меню камеры. */
CamParamCatalog makeE650()
{
    CamParamCatalog c = makeStudio();
    c.shutter = choicesOf(kE650Shutter, sizeof(kE650Shutter) / sizeof(kE650Shutter[0]));
    c.agc_max = choicesOf(kE650AgcMax, sizeof(kE650AgcMax) / sizeof(kE650AgcMax[0]));
    return c;
}

/** Студийный AWC, HE-шкала AGC. HE120/HE60/HE50: своего короткого AWC в библиотеке нет. */
CamParamCatalog makeHeOptics()
{
    CamParamCatalog c = makeStudio();
    c.agc_max = choicesOf(kHeAgcMax, sizeof(kHeAgcMax) / sizeof(kHeAgcMax[0]));
    // HE50/HE60/HE120: OMS 001h…0FFh (≈50.16Hz…542Hz / 60.17Hz…644Hz).
    c.synchro = hexRangeOf(1, 0x0FF, 3);
    return c;
}

/** HE130 / HE870: AWC 0…5, Var = 5. */
CamParamCatalog makeHeAwc()
{
    CamParamCatalog c = makeHeOptics();
    c.awc = choicesOf(kHeAwc, sizeof(kHeAwc) / sizeof(kHeAwc[0]));
    // HE130: OMS 001h…0FFh (как HE50/60/120).
    return c;
}

/** AW-HE870: Synchro шире, чем у HE130. */
CamParamCatalog makeHe870()
{
    CamParamCatalog c = makeHeAwc();
    // HE870 N: 001h=61.19Hz…109h=1466Hz; E/MC: 001h=50.99Hz…10Dh=1478Hz.
    c.synchro = hexRangeOf(1, 0x10D, 3);
    return c;
}

/** UE150: AWC как у HE, сцена Scene 1…4. */
CamParamCatalog makeUe()
{
    CamParamCatalog c = makeHeAwc();
    c.scene = choicesOf(kUeScene, sizeof(kUeScene) / sizeof(kUeScene[0]));
    return c;
}

const CamParamCatalog& studioCat()
{
    static const CamParamCatalog cat = makeStudio();
    return cat;
}

const CamParamCatalog& e650Cat()
{
    static const CamParamCatalog cat = makeE650();
    return cat;
}

const CamParamCatalog& heOpticsCat()
{
    static const CamParamCatalog cat = makeHeOptics();
    return cat;
}

const CamParamCatalog& heAwcCat()
{
    static const CamParamCatalog cat = makeHeAwc();
    return cat;
}

const CamParamCatalog& he870Cat()
{
    static const CamParamCatalog cat = makeHe870();
    return cat;
}

const CamParamCatalog& ueCat()
{
    static const CamParamCatalog cat = makeUe();
    return cat;
}

int append(char* out, size_t cap, size_t& o, const char* s)
{
    if (!s) {
        return 0;
    }
    const size_t n = std::strlen(s);
    if (o + n >= cap) {
        return -1;
    }
    std::memcpy(out + o, s, n);
    o += n;
    out[o] = '\0';
    return 0;
}

int appendf(char* out, size_t cap, size_t& o, const char* fmt, ...)
{
    char tmp[192];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    return append(out, cap, o, tmp);
}

int appendChoiceItems(char* out, size_t cap, size_t& o, const CamParamSpec& s)
{
    if (append(out, cap, o, "\"items\":[") != 0) {
        return -1;
    }
    for (size_t i = 0; i < s.choice_count; ++i) {
        if (i > 0 && append(out, cap, o, ",") != 0) {
            return -1;
        }
        const CamChoice& c = s.choices[i];
        if (appendf(out, cap, o, "{\"v\":\"%s\",\"l\":\"%s\"}", c.wire, c.label) != 0) {
            return -1;
        }
    }
    return append(out, cap, o, "]");
}

int appendSpec(char* out, size_t cap, size_t& o, const char* key, const CamParamSpec& s)
{
    if (appendf(out, cap, o, "\"%s\":{", key) != 0) {
        return -1;
    }
    if (s.kind == CamParamKind::HexRange) {
        if (appendf(
                out, cap, o, "\"kind\":\"hex_range\",\"min\":%u,\"max\":%u,\"digits\":%u",
                static_cast<unsigned>(s.min_u), static_cast<unsigned>(s.max_u),
                static_cast<unsigned>(s.hex_digits))
            != 0) {
            return -1;
        }
    } else {
        if (append(out, cap, o, "\"kind\":\"choices\",") != 0) {
            return -1;
        }
        if (appendChoiceItems(out, cap, o, s) != 0) {
            return -1;
        }
    }
    return append(out, cap, o, "}");
}

} // namespace

const CamParamCatalog& camParamCatalog(CameraModelId id)
{
    switch (id) {
    case CameraModelId::Ue150:
        return ueCat();
    case CameraModelId::He870:
        return he870Cat();
    case CameraModelId::He130:
        return heAwcCat();
    case CameraModelId::He120:
    case CameraModelId::He60:
    case CameraModelId::He50:
        return heOpticsCat();
    case CameraModelId::E650:
        return e650Cat();
    case CameraModelId::E600:
    case CameraModelId::Generic:
    default:
        return studioCat();
    }
}

int camParamCatalogToJson(const CamParamCatalog& cat, char* out, size_t out_cap)
{
    if (!out || out_cap < 8) {
        return -1;
    }
    size_t o = 0;
    out[0] = '\0';
    const struct {
        const char* key;
        const CamParamSpec* spec;
    } fields[] = {
        {"awc", &cat.awc},
        {"scene", &cat.scene},
        {"shutter", &cat.shutter},
        {"synchro", &cat.synchro},
        {"gamma", &cat.gamma},
        {"agc_max", &cat.agc_max},
        {"total_dtl", &cat.total_dtl},
        {"genlock", &cat.genlock},
        {"colorbar", &cat.colorbar},
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        if (i > 0 && append(out, out_cap, o, ",") != 0) {
            return -1;
        }
        if (appendSpec(out, out_cap, o, fields[i].key, *fields[i].spec) != 0) {
            return -1;
        }
    }
    return static_cast<int>(o);
}

} // namespace ccam::devices
