#include "devices/osd_params.hpp"

#include "catalog/osd_menu.hpp"
#include "devices/camera_device.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ccam::devices {
namespace {

const CamChoice* agcChoices(CameraModelId id, uint8_t& n)
{
    if (camUsesHeAgc(id)) {
        n = static_cast<uint8_t>(sizeof(catalog::kOsdAgcHe) / sizeof(catalog::kOsdAgcHe[0]));
        return catalog::kOsdAgcHe;
    }
    n = static_cast<uint8_t>(sizeof(catalog::kOsdAgcStudio) / sizeof(catalog::kOsdAgcStudio[0]));
    return catalog::kOsdAgcStudio;
}

#define CCAM_OSD_LIN(name, key, grp, title, note, a, b, bias, unit)                               \
    {                                                                                              \
        catalog::OsdItem::name, key, grp, title, note, true, static_cast<uint8_t>(a),             \
            static_cast<uint8_t>(b), static_cast<int16_t>(bias), unit, nullptr, nullptr, nullptr, \
            0, false                                                                               \
    },
#define CCAM_OSD_END(name, key, grp, title, note, a, b, lo, hi)                                   \
    {                                                                                              \
        catalog::OsdItem::name, key, grp, title, note, true, static_cast<uint8_t>(a),             \
            static_cast<uint8_t>(b), 0, nullptr, lo, hi, nullptr, 0, false                         \
    },
#define CCAM_OSD_CHOICE(name, key, grp, title, note, arr)                                         \
    {                                                                                              \
        catalog::OsdItem::name, key, grp, title, note, false, 0, 0, 0, nullptr, nullptr, nullptr, \
            arr, static_cast<uint8_t>(sizeof(arr) / sizeof((arr)[0])), false                      \
    },
#define CCAM_OSD_AGC(name, key, grp, title, note)                                                 \
    {                                                                                              \
        catalog::OsdItem::name, key, grp, title, note, false, 0, 0, 0, nullptr, nullptr, nullptr, \
            nullptr, 0, true                                                                       \
    },
#define CCAM_OSD_PARAM(name, code, key, grp, title, note, kind, ...)                               \
    CCAM_OSD_##kind(name, key, grp, title, note, ##__VA_ARGS__)

const OsdParam kParams[] = {
    CCAM_FOR_EACH_OSD_ITEM(CCAM_OSD_PARAM)
};

#undef CCAM_OSD_PARAM
#undef CCAM_OSD_AGC
#undef CCAM_OSD_CHOICE
#undef CCAM_OSD_END
#undef CCAM_OSD_LIN

static_assert(sizeof(kParams) / sizeof(kParams[0]) == static_cast<size_t>(catalog::OsdItem::Count),
              "osd param table vs OsdItem");

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
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(tmp)) {
        return -1;
    }
    return append(out, cap, o, tmp);
}

bool hexByte(const char* s, uint8_t& value, char out[3])
{
    if (!s) {
        return false;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    if (!s[0] || !s[1] || s[2] != '\0') {
        return false;
    }
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        return -1;
    };
    const int hi = nib(s[0]);
    const int lo = nib(s[1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    value = static_cast<uint8_t>((hi << 4) | lo);
    out[0] = "0123456789ABCDEF"[hi];
    out[1] = "0123456789ABCDEF"[lo];
    out[2] = '\0';
    return true;
}

const CamChoice* choicesOf(const OsdParam& param, CameraModelId model, uint8_t& n)
{
    if (param.agc) {
        return agcChoices(model, n);
    }
    n = param.choice_count;
    return param.choices;
}

} // namespace

const OsdParam* osdParamTable(size_t& count)
{
    count = sizeof(kParams) / sizeof(kParams[0]);
    return kParams;
}

const OsdParam* osdParamByKey(const char* key)
{
    if (!key || !key[0]) {
        return nullptr;
    }
    for (const OsdParam& p : kParams) {
        if (std::strcmp(p.key, key) == 0) {
            return &p;
        }
    }
    return nullptr;
}

bool osdParamWire(const OsdParam& param, CameraModelId model, const char* wire, char out[3])
{
    uint8_t value = 0;
    if (!hexByte(wire, value, out)) {
        return false;
    }
    if (param.linear) {
        return value >= param.min_u && value <= param.max_u;
    }
    uint8_t n = 0;
    const CamChoice* choices = choicesOf(param, model, n);
    for (uint8_t i = 0; i < n; ++i) {
        if (std::strcmp(choices[i].wire, out) == 0) {
            return true;
        }
    }
    return false;
}

ccam::Status setOsdParam(CameraDeviceBase& cam, const char* key, const char* wire)
{
    const OsdParam* param = osdParamByKey(key);
    if (!param) {
        return ccam::Status::Param;
    }
    char data[3];
    if (!osdParamWire(*param, cam.modelId(), wire, data)) {
        return ccam::Status::Param;
    }
    // KNEE POINT: PDF control ≠ question. UI/QSD: FF=Dynamic, 00…0A=88…98%.
    // OSD set: 00=Dynamic, 01…0B=88…98%. Live E650: SET 00→FF, SET n+1→n.
    if (param->item == catalog::OsdItem::KneePoint) {
        uint8_t q = 0;
        char tmp[3];
        if (!hexByte(data, q, tmp)) {
            return ccam::Status::Param;
        }
        uint8_t c = 0;
        if (q == 0xFF) {
            c = 0x00;
        } else if (q <= 0x0A) {
            c = static_cast<uint8_t>(q + 1u);
        } else {
            return ccam::Status::Param;
        }
        data[0] = "0123456789ABCDEF"[(c >> 4) & 0x0F];
        data[1] = "0123456789ABCDEF"[c & 0x0F];
        data[2] = '\0';
    }
    return cam.setOsd(param->item, data);
}

ccam::Status getOsdParam(CameraDeviceBase& cam, const char* key, char* out, size_t out_cap)
{
    const OsdParam* param = osdParamByKey(key);
    if (!param || !out || out_cap == 0) {
        return ccam::Status::Param;
    }
    return cam.requestOsd(param->item, out, out_cap);
}

int osdParamCatalogToJson(CameraModelId model, char* out, size_t out_cap)
{
    if (!out || out_cap < 2) {
        return -1;
    }
    size_t o = 0;
    out[0] = '\0';
    if (append(out, out_cap, o, "[") != 0) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(kParams) / sizeof(kParams[0]); ++i) {
        const OsdParam& p = kParams[i];
        if (i > 0 && append(out, out_cap, o, ",") != 0) {
            return -1;
        }
        if (appendf(out, out_cap, o,
                    "{\"key\":\"%s\",\"code\":\"%02X\",\"group\":\"%s\",\"name\":\"%s\"",
                    p.key, static_cast<unsigned>(catalog::subcode(p.item)), p.group, p.name)
            != 0) {
            return -1;
        }
        if (p.note && appendf(out, out_cap, o, ",\"note\":\"%s\"", p.note) != 0) {
            return -1;
        }
        if (p.linear) {
            if (appendf(out, out_cap, o, ",\"kind\":\"linear\",\"min\":%u,\"max\":%u,\"bias\":%d",
                        static_cast<unsigned>(p.min_u), static_cast<unsigned>(p.max_u),
                        static_cast<int>(p.bias))
                != 0) {
                return -1;
            }
            if (p.unit && appendf(out, out_cap, o, ",\"unit\":\"%s\"", p.unit) != 0) {
                return -1;
            }
            if (p.end_lo && appendf(out, out_cap, o, ",\"lo\":\"%s\",\"hi\":\"%s\"", p.end_lo, p.end_hi) != 0) {
                return -1;
            }
        } else {
            uint8_t n = 0;
            const CamChoice* choices = choicesOf(p, model, n);
            if (append(out, out_cap, o, ",\"kind\":\"choices\",\"items\":[") != 0) {
                return -1;
            }
            for (uint8_t c = 0; c < n; ++c) {
                if (c > 0 && append(out, out_cap, o, ",") != 0) {
                    return -1;
                }
                if (appendf(out, out_cap, o, "{\"v\":\"%s\",\"l\":\"%s\"}", choices[c].wire, choices[c].label) != 0) {
                    return -1;
                }
            }
            if (append(out, out_cap, o, "]") != 0) {
                return -1;
            }
        }
        if (append(out, out_cap, o, "}") != 0) {
            return -1;
        }
    }
    if (append(out, out_cap, o, "]") != 0) {
        return -1;
    }
    return static_cast<int>(o);
}

} // namespace ccam::devices
