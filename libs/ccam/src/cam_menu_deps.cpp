#include "devices/cam_menu_deps.hpp"

#include <cstdio>
#include <cstring>

namespace ccam::devices {
namespace {

using P = CamMenuPhase;
using C = CamMenuCap;
using B = CamMenuBank;
using F = CamMenuFmt;
using Cam = catalog::CameraCmd;
using Osd = catalog::OsdItem;
using Vid = catalog::VideoMenuItem;

#define CAM(key, phase, gate, cap, cmd, fmt)                                                       \
    {key, P::phase, gate, C::cap, B::Cam, camMenuItem(Cam::cmd), F::fmt},
#define OSD(key, phase, gate, cap, cmd, fmt)                                                       \
    {key, P::phase, gate, C::cap, B::Osd, camMenuItem(Osd::cmd), F::fmt},
#define VID(key, phase, gate, cap, cmd, fmt)                                                       \
    {key, P::phase, gate, C::cap, B::Video, camMenuItem(Vid::cmd), F::fmt},

const CamMenuPollEntry kPoll[] = {
    /* Primary */
    CAM("gain_mode", Primary, nullptr, None, GainUp, GainMode)
    CAM("shutter", Primary, nullptr, None, Shutter, TrackShutter)
    CAM("iris_am", Primary, nullptr, None, IrisAutoManual, IrisAm)
    CAM("awc", Primary, nullptr, None, AwcMode, TrackAwc)
    CAM("scene", Primary, nullptr, None, SceneFile, DecU8)

    /* Dep */
    CAM("iris", Dep, "iris:manual", None, ManualIrisVolume, IrisNorm)
    CAM("gain_db", Dep, "gain:manual", None, GainUp, GainDb)
    CAM("dig_gain", Dep, "gain:manual", None, DigitalGainUp, DecU8)
    OSD("agc_max", Dep, "gain:agc", AgcMax, AgcMax, OsdWire)
    CAM("synchro", Dep, "shut:synchro", Synchro, SynchroScan, Synchro)
    OSD("atw", Dep, "awc:atw", None, AtwSpeed, OsdWire)
    CAM("r_gain", Dep, "awc:mem", Paint, RGain, HexU16)
    CAM("b_gain", Dep, "awc:mem", Paint, BGain, HexU16)
    CAM("r_ped", Dep, "awc:nopreset", Paint, RPedestal, HexU16)
    CAM("b_ped", Dep, "awc:nopreset", Paint, BPedestal, HexU16)

    /* Always */
    CAM("light_peak", Always, nullptr, None, LightPeakAvg, DecU8)
    CAM("light_area", Always, nullptr, None, LightArea, DecU8)
    CAM("t_ped", Always, nullptr, Paint, TPedestal, HexU16)
    OSD("airis", Always, nullptr, None, AirisLevel, AirisSigned)
    CAM("chroma", Always, nullptr, Chroma, ChromaLevel, ChromaSigned)
    CAM("detail", Always, nullptr, None, Detail, DecU8)
    CAM("hd_detail", Always, nullptr, HdDetail, HdDetail, DecU8)
    VID("total_dtl", Always, nullptr, TotalDtl, TotalDtlLevel, VideoWire)
    OSD("gamma", Always, nullptr, Gamma, Gamma, OsdWire)
    OSD("knee", Always, nullptr, None, KneePoint, OsdWire)
    OSD("cdnr", Always, nullptr, None, CleanDnr, OsdWire)
    CAM("dnr3d", Always, nullptr, None, Dnr3d, DecU8)
    OSD("wclip", Always, nullptr, None, WhiteClip, OsdHexU)
    CAM("colorbar", Always, nullptr, None, ColorBar, ColorBar)
    CAM("filter", Always, nullptr, None, FilterNd, DecU8)
    CAM("af", Always, nullptr, None, AutoFocus, Switch)
    CAM("fieldframe", Always, nullptr, FieldFrame, FieldFrame, FieldFrame)
    CAM("negpos", Always, nullptr, NegPos, NegPos, Switch)
    VID("genlock", Always, nullptr, Genlock, GenlockInput, VideoWire)
    CAM("menu", Always, nullptr, None, MenuOnOff, Switch)
    /* Matrix R-G…B-G (OSD:2F) — не UI меню E650; Color Matrix Set = OSD:80+ Gain/Phase. */
    CAM("model", Always, nullptr, None, ModelNumber, Model)
    CAM("version", Always, nullptr, None, SoftwareVersion, Version)
};

#undef CAM
#undef OSD
#undef VID

bool csvHasToken(const char* csv, const char* token)
{
    if (!csv || !csv[0] || !token || !token[0]) {
        return false;
    }
    const size_t tn = std::strlen(token);
    for (const char* p = csv; *p;) {
        while (*p == ',' || *p == ' ') {
            ++p;
        }
        if (!*p) {
            break;
        }
        const char* e = p;
        while (*e && *e != ',') {
            ++e;
        }
        size_t n = static_cast<size_t>(e - p);
        while (n > 0 && p[n - 1] == ' ') {
            --n;
        }
        if (n == tn && std::strncmp(p, token, tn) == 0) {
            return true;
        }
        p = e;
    }
    return false;
}

} // namespace

const CamMenuPollEntry* camMenuPoll(size_t& count)
{
    count = sizeof(kPoll) / sizeof(kPoll[0]);
    return kPoll;
}

const CamMenuPollEntry* camMenuDeps(size_t& count)
{
    static size_t dep_off = 0;
    static size_t dep_n = 0;
    static bool once = false;
    if (!once) {
        once = true;
        size_t n = 0;
        const CamMenuPollEntry* all = camMenuPoll(n);
        for (size_t i = 0; i < n; ++i) {
            if (all[i].phase == CamMenuPhase::Dep) {
                if (dep_n == 0) {
                    dep_off = i;
                }
                ++dep_n;
            } else if (dep_n > 0) {
                break;
            }
        }
    }
    count = dep_n;
    return kPoll + dep_off;
}

bool camMenuGateActive(const char* gate, const CamMenuPrimaryState& st)
{
    if (!gate || !gate[0]) {
        return true;
    }
    if (std::strcmp(gate, "iris:manual") == 0) {
        return st.iris_am == 0;
    }
    if (std::strcmp(gate, "iris:auto") == 0) {
        return st.iris_am == 1;
    }
    if (std::strcmp(gate, "gain:manual") == 0) {
        return !st.gain_agc;
    }
    if (std::strcmp(gate, "gain:agc") == 0) {
        return st.gain_agc;
    }
    if (std::strcmp(gate, "shut:synchro") == 0) {
        return st.shutter == st.shutter_synchro;
    }
    if (std::strcmp(gate, "awc:atw") == 0) {
        return st.awc == 0;
    }
    if (std::strcmp(gate, "awc:mem") == 0) {
        return st.he_awc ? (st.awc == 1 || st.awc == 2) : (st.awc == 2 || st.awc == 3);
    }
    if (std::strcmp(gate, "awc:nopreset") == 0) {
        const bool preset =
            st.he_awc ? (st.awc == 3 || st.awc == 4) : (st.awc == 4 || st.awc == 5);
        return !preset;
    }
    return false;
}

bool camMenuCapOk(CamMenuCap cap, const CamCaps& caps)
{
    switch (cap) {
    case CamMenuCap::None:
        return true;
    case CamMenuCap::Chroma:
        return caps.chroma;
    case CamMenuCap::Paint:
        return caps.paint;
    case CamMenuCap::Synchro:
        return caps.synchro;
    case CamMenuCap::AgcMax:
        return caps.agc_max;
    case CamMenuCap::Gamma:
        return caps.gamma;
    case CamMenuCap::HdDetail:
        return caps.hd_detail;
    case CamMenuCap::TotalDtl:
        return caps.total_dtl;
    case CamMenuCap::Genlock:
        return caps.genlock;
    case CamMenuCap::NegPos:
        return caps.neg_pos;
    case CamMenuCap::FieldFrame:
        return caps.field_frame;
    default:
        return true;
    }
}

bool camMenuEntryWanted(
    const CamMenuPollEntry& e,
    const CamMenuPrimaryState& st,
    const CamCaps& caps,
    const char* only_csv,
    const char* gates_csv)
{
    if (!camMenuCapOk(e.cap, caps)) {
        return false;
    }
    if (!camMenuGateActive(e.gate, st)) {
        return false;
    }
    if (gates_csv && gates_csv[0]) {
        if (!e.gate || !csvHasToken(gates_csv, e.gate)) {
            return false;
        }
    }
    if (only_csv && only_csv[0] && !csvHasToken(only_csv, e.key)) {
        return false;
    }
    return true;
}

int camMenuDepsToJson(char* out, size_t out_cap)
{
    if (!out || out_cap < 4) {
        return -1;
    }
    size_t o = 0;
    auto app = [&](const char* s) -> int {
        if (!s) {
            return 0;
        }
        const size_t n = std::strlen(s);
        if (o + n >= out_cap) {
            return -1;
        }
        std::memcpy(out + o, s, n);
        o += n;
        out[o] = '\0';
        return 0;
    };
    auto appf = [&](const char* fmt, const char* a, const char* b) -> int {
        char tmp[96];
        const int n = std::snprintf(tmp, sizeof(tmp), fmt, a, b);
        if (n < 0) {
            return -1;
        }
        return app(tmp);
    };

    if (app("[") != 0) {
        return -1;
    }
    size_t n = 0;
    const CamMenuPollEntry* deps = camMenuDeps(n);
    for (size_t i = 0; i < n; ++i) {
        if (i > 0 && app(",") != 0) {
            return -1;
        }
        if (appf("{\"key\":\"%s\",\"gate\":\"%s\"}", deps[i].key, deps[i].gate ? deps[i].gate : "")
            != 0) {
            return -1;
        }
    }
    if (app("]") != 0) {
        return -1;
    }
    return static_cast<int>(o);
}

} // namespace ccam::devices
