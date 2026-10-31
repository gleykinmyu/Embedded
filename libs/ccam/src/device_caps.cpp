#include "devices/device_caps.hpp"

#include <cstring>

namespace ccam::devices {
namespace {

PtCaps capsIntegrated()
{
    PtCaps c{};
    c.abs_pt = true;
    c.abs_pt_speed = true;
    c.preset_map = true;
    c.limit_ctrl = true;
    c.limit_toggle = true;
    c.tally = true;
    c.software_ver = true;
    c.pos_disp = true;
    c.pt_pos = true;
    c.lens_q = true;
    c.power_cam_tx = true;
    return c;
}

PtCaps capsPhHead()
{
    PtCaps c{};
    c.limit_toggle = true;
    c.tally = true;
    c.fan = true;
    c.wiper = true;
    c.washer = true;
    c.wireless = true;
    c.option = true;
    c.tilt_range = true;
    c.heater = true;
    c.software_ver = true;
    return c;
}

} // namespace

const char* ptModelKey(PtModelId id)
{
    switch (id) {
    case PtModelId::Ph350:
        return "ph350";
    case PtModelId::Ph360:
        return "ph360";
    case PtModelId::Ph650:
        return "ph650";
    case PtModelId::Ph405:
        return "ph405";
    case PtModelId::He130:
        return "he130";
    case PtModelId::He870:
        return "he870";
    case PtModelId::Ue150:
        return "ue150";
    case PtModelId::IntegratedPtz:
        return "iptz";
    case PtModelId::Generic:
    default:
        return "generic";
    }
}

const char* camModelKey(CameraModelId id)
{
    switch (id) {
    case CameraModelId::E650:
        return "e650";
    case CameraModelId::E600:
        return "e600";
    case CameraModelId::He130:
        return "he130";
    case CameraModelId::He870:
        return "he870";
    case CameraModelId::Ue150:
        return "ue150";
    case CameraModelId::Generic:
    default:
        return "generic";
    }
}

const char* ptModelLabel(PtModelId id)
{
    switch (id) {
    case PtModelId::Ph350:
        return "AW-PH350E";
    case PtModelId::Ph360:
        return "AW-PH360";
    case PtModelId::Ph650:
        return "AW-PH650";
    case PtModelId::Ph405:
        return "AW-PH405";
    case PtModelId::He130:
        return "AW-HE130 Pan/Tilt";
    case PtModelId::He870:
        return "AW-HE870 Pan/Tilt";
    case PtModelId::Ue150:
        return "AW-UE150 Pan/Tilt";
    case PtModelId::IntegratedPtz:
        return "Integrated PTZ";
    case PtModelId::Generic:
    default:
        return "Generic Pan/Tilt";
    }
}

const char* camModelLabel(CameraModelId id)
{
    switch (id) {
    case CameraModelId::E650:
        return "AW-E650E";
    case CameraModelId::E600:
        return "AW-E600";
    case CameraModelId::He130:
        return "AW-HE130";
    case CameraModelId::He870:
        return "AW-HE870";
    case CameraModelId::Ue150:
        return "AW-UE150";
    case CameraModelId::Generic:
    default:
        return "Generic Camera";
    }
}

PtModelId ptModelFromKey(const char* key)
{
    if (!key || !key[0]) {
        return PtModelId::Ph350;
    }
    if (std::strcmp(key, "ph350") == 0) {
        return PtModelId::Ph350;
    }
    if (std::strcmp(key, "ph360") == 0) {
        return PtModelId::Ph360;
    }
    if (std::strcmp(key, "ph650") == 0) {
        return PtModelId::Ph650;
    }
    if (std::strcmp(key, "ph405") == 0) {
        return PtModelId::Ph405;
    }
    if (std::strcmp(key, "he130") == 0) {
        return PtModelId::He130;
    }
    if (std::strcmp(key, "he870") == 0) {
        return PtModelId::He870;
    }
    if (std::strcmp(key, "ue150") == 0) {
        return PtModelId::Ue150;
    }
    if (std::strcmp(key, "iptz") == 0) {
        return PtModelId::IntegratedPtz;
    }
    return PtModelId::Ph350;
}

CameraModelId camModelFromKey(const char* key)
{
    if (!key || !key[0]) {
        return CameraModelId::E650;
    }
    if (std::strcmp(key, "e650") == 0) {
        return CameraModelId::E650;
    }
    if (std::strcmp(key, "e600") == 0) {
        return CameraModelId::E600;
    }
    if (std::strcmp(key, "he130") == 0) {
        return CameraModelId::He130;
    }
    if (std::strcmp(key, "he870") == 0) {
        return CameraModelId::He870;
    }
    if (std::strcmp(key, "ue150") == 0) {
        return CameraModelId::Ue150;
    }
    return CameraModelId::E650;
}

PtCaps ptCapsFor(PtModelId id)
{
    switch (id) {
    case PtModelId::Ph350: {
        PtCaps c = capsPhHead();
        c.defroster = true;
        return c;
    }
    case PtModelId::Ph360:
    case PtModelId::Ph650:
    case PtModelId::Ph405:
        return capsPhHead();
    case PtModelId::He130:
    case PtModelId::He870:
    case PtModelId::Ue150:
    case PtModelId::IntegratedPtz:
        return capsIntegrated();
    case PtModelId::Generic:
    default:
        return PtCaps{};
    }
}

CamCaps camCapsFor(CameraModelId id)
{
    CamCaps c{};
    switch (id) {
    case CameraModelId::E650:
        c.neg_pos = true;
        c.field_frame = true;
        c.chroma = true;
        c.black_shade = true;
        c.synchro = true;
        c.abc = true;
        c.paint = true;
        c.genlock = true;
        c.gamma = true;
        c.agc_max = true;
        c.total_dtl = true;
        c.monitor = true;
        c.lens_h = true;
        break;
    case CameraModelId::E600:
        c.neg_pos = true;
        c.field_frame = true;
        c.chroma = true;
        c.synchro = true;
        c.abc = true;
        c.paint = true;
        c.genlock = true;
        c.gamma = true;
        c.agc_max = true;
        c.total_dtl = true;
        c.monitor = true;
        c.lens_h = true;
        break;
    case CameraModelId::He870:
        c.hd_detail = true;
        c.neg_pos = true;
        c.chroma = true;
        c.synchro = true;
        c.abc = true;
        c.paint = true;
        c.gamma = true;
        c.agc_max = true;
        c.monitor = true;
        break;
    case CameraModelId::He130:
        c.paint = true;
        c.gamma = true;
        c.monitor = true;
        c.menu_lr = true; // DLT/DRT
        break;
    case CameraModelId::Ue150:
        c.paint = true;
        c.gamma = true;
        c.monitor = true;
        break;
    case CameraModelId::Generic:
    default:
        break;
    }
    return c;
}

} // namespace ccam::devices
