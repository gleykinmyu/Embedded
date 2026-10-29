#include "devices/pt_link_health.hpp"

namespace ccam::devices {
namespace {

bool due(uint32_t now, uint32_t last, uint32_t interval)
{
    // wrap-safe
    return static_cast<uint32_t>(now - last) >= interval;
}

} // namespace

void PtLinkHealth::tick(uint32_t now_ms)
{
    if (pt_ == nullptr) {
        return;
    }
    if (last_poll_ms_ != 0 && !due(now_ms, last_poll_ms_, interval_ms_)) {
        return;
    }
    (void)doPoll(now_ms);
}

Status PtLinkHealth::pollNow(uint32_t now_ms)
{
    if (pt_ == nullptr) {
        return Status::Param;
    }
    return doPoll(now_ms);
}

Status PtLinkHealth::doPoll(uint32_t now_ms)
{
    last_poll_ms_ = now_ms;
    uint8_t code = 0xFF;
    const Status st = pt_->requestPower(code);
    last_st_ = st;
    if (st == Status::Ok) {
        fails_ = 0;
        alive_ = true;
        power_code_ = code;
        last_ok_ms_ = now_ms;
        return st;
    }
    if (fails_ < 255) {
        ++fails_;
    }
    if (fails_ >= fail_limit_) {
        alive_ = false;
        // power_code_ оставляем последним известным
    }
    return st;
}

} // namespace ccam::devices
