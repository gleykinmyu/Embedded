#include "app/ptz_operator.hpp"

namespace app {

PtzOperator::PtzOperator(ccam::devices::IPtDevice& pt, ccam::devices::ICameraDevice& camera)
    : pt_(pt)
    , camera_(camera)
{
}

void PtzOperator::applyJoystick(const JoystickAxes& joy, const PanelSettings& panel)
{
    if (!pt_power_on_) {
        return;
    }

    if (joy.button) {
        (void)pt_.stopAll();
        return;
    }

    const uint8_t pan_speed = axisToPtSpeed(joy.pan, panel.pan_tilt_rate);
    const uint8_t tilt_speed = axisToPtSpeed(joy.tilt, panel.pan_tilt_rate);

    if (pan_speed == ccam::kPtSpeedStop && tilt_speed == ccam::kPtSpeedStop) {
        (void)pt_.panStop();
        (void)pt_.tiltStop();
    } else {
        (void)pt_.setPanTiltSpeed(pan_speed, tilt_speed);
    }

    const uint8_t zoom_speed = axisToPtSpeed(joy.zoom, panel.zoom_focus_rate);
    if (zoom_speed == ccam::kPtSpeedStop) {
        (void)pt_.zoomStop();
    } else {
        (void)pt_.setZoomSpeed(zoom_speed);
    }

    const uint8_t focus_speed = axisToPtSpeed(joy.focus, panel.zoom_focus_rate);
    if (focus_speed == ccam::kPtSpeedStop) {
        (void)pt_.focusStop();
    } else {
        (void)pt_.setFocusSpeed(focus_speed);
    }
}

void PtzOperator::applyTouchAction(TouchAction action, PanelSettings& panel)
{
    switch (action) {
    case TouchAction::None:
        break;

    case TouchAction::StopAll:
        (void)pt_.stopAll();
        break;

    case TouchAction::RecallPreset:
        (void)pt_.recallPreset(panel.preset);
        break;

    case TouchAction::SavePreset:
        (void)pt_.savePreset(panel.preset);
        break;

    case TouchAction::AwcStart:
        (void)camera_.setAwcMode(panel.awc_mode);
        (void)camera_.awcStart();
        break;

    case TouchAction::PowerToggle:
        panel.pt_power_on = !panel.pt_power_on;
        pt_power_on_ = panel.pt_power_on;
        (void)pt_.power(
            pt_power_on_ ? ccam::PtPowerMode::On : ccam::PtPowerMode::Off);
        if (!pt_power_on_) {
            (void)pt_.stopAll();
        }
        break;

    case TouchAction::QueryPosition: {
        ccam::PtPosition pos{};
        if (pt_.queryPosition(pos) == ccam::Status::Ok) {
            last_position_ = pos;
            has_position_ = true;
        }
        break;
    }
    }
}

void PtzOperator::tick(const JoystickAxes& joy, PanelSettings& panel)
{
    if (panel.pt_power_on != pt_power_on_) {
        pt_power_on_ = panel.pt_power_on;
        (void)pt_.power(
            pt_power_on_ ? ccam::PtPowerMode::On : ccam::PtPowerMode::Off);
    }

    if (panel.pending != TouchAction::None) {
        applyTouchAction(panel.pending, panel);
        panel.pending = TouchAction::None;
    }

    applyJoystick(joy, panel);
}

} // namespace app
