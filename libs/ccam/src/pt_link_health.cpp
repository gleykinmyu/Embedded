#include "devices/pt_link_health.hpp"

namespace ccam::devices {

const char* PtLinkHealth::powerLabel() const
{
    return powerStatusLabel(power_code_);
}

void PtLinkHealth::clearOptics()
{
    optics_.clear();
}

void PtLinkHealth::clearCam()
{
    cam_.clear();
    cam_id_[0] = '\0';
}

void PtLinkHealth::tick(uint32_t now_ms)
{
    if (pt_dev_ == nullptr) {
        return;
    }
    if (!pt_.due(now_ms)) {
        return;
    }
    (void)doPoll(now_ms);
}

Status PtLinkHealth::pollNow(uint32_t now_ms)
{
    if (pt_dev_ == nullptr) {
        return Status::Param;
    }
    return doPoll(now_ms);
}

Status PtLinkHealth::doPoll(uint32_t now_ms)
{
    uint8_t code = 0xFF;
    const Status st = pt_dev_->requestPower(code);
    pt_.last = st;
    if (st == Status::Ok) {
        pt_.markOk(now_ms);
        power_code_ = code;

        // #GZ: hex = головка в норме; "---" / Io = выкл / оптика не готова (не QID).
        pollOptics(now_ms);

        if (!camTxLikely()) {
            if (power_code_ != kPowerStarting) {
                clearCam();
            }
        } else if (cam_dev_ != nullptr && cam_.due(now_ms)) {
            pollCam(now_ms);
        }
        return st;
    }

    pt_.markFail(now_ms);
    if (!pt_.alive) {
        clearOptics();
        clearCam();
        error_code_ = 0xFF;
    }
    return st;
}

void PtLinkHealth::pollOptics(uint32_t now_ms)
{
    if (pt_dev_ == nullptr) {
        return;
    }
    uint16_t zoom = 0;
    const Status st = pt_dev_->getZoom(zoom);
    optics_.last = st;
    if (st == Status::Ok) {
        optics_.markOk(now_ms);
        return;
    }
    optics_.markFail(now_ms);
    if (!optics_.alive) {
        clearCam();
    }
}

void PtLinkHealth::pollCam(uint32_t now_ms)
{
    if (cam_dev_ == nullptr) {
        return;
    }
    char id[sizeof(cam_id_)] = {};
    const Status st = cam_dev_->requestModel(id, sizeof(id));
    cam_.last = st;
    if (st == Status::Ok && id[0] != '\0') {
        cam_.markOk(now_ms);
        std::strncpy(cam_id_, id, sizeof(cam_id_) - 1);
        cam_id_[sizeof(cam_id_) - 1] = '\0';
        return;
    }
    cam_.markFail(now_ms);
}

} // namespace ccam::devices
