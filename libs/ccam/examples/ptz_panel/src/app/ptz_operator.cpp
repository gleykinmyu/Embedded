#include "app/ptz_operator.hpp"

namespace app {

PtzOperator::PtzOperator(pt::IDevice& pt, ccam::devices::ICameraDevice& camera)
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
        (void)pt_.stop();
        return;
    }

    const int8_t pan = axisToSignedSpeed(joy.pan, panel.pan_tilt_rate);
    const int8_t tilt = axisToSignedSpeed(joy.tilt, panel.pan_tilt_rate);
    (void)pt_.movePT(pan, tilt);

    (void)pt_.move(pt::Axis::Zoom, axisToSignedSpeed(joy.zoom, panel.zoom_focus_rate));
    (void)pt_.move(pt::Axis::Focus, axisToSignedSpeed(joy.focus, panel.zoom_focus_rate));
}

void PtzOperator::applyTouchAction(TouchAction action, PanelSettings& panel)
{
    switch (action) {
    case TouchAction::None:
        break;

    case TouchAction::StopAll:
        (void)pt_.stop();
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
        (void)pt_.power(pt_power_on_ ? pt::PowerMode::On : pt::PowerMode::Off);
        if (!pt_power_on_) {
            (void)pt_.stop();
        }
        break;

    case TouchAction::QueryPosition: {
        pt::Position pos{};
        if (pt_.requestPosition(pos) == pt::Status::Ok) {
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
        (void)pt_.power(pt_power_on_ ? pt::PowerMode::On : pt::PowerMode::Off);
    }

    if (panel.pending != TouchAction::None) {
        applyTouchAction(panel.pending, panel);
        panel.pending = TouchAction::None;
    }

    applyJoystick(joy, panel);
}

} // namespace app
