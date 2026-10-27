/**
 * @file ptz_operator.hpp
 * @brief ?????? ????????? + ????????? ?????? ? pt::IDevice / ICameraDevice.
 */

#pragma once

#include "app/joystick.hpp"
#include "app/touch_panel.hpp"
#include "devices/camera_device.hpp"
#include "devices/ipt_device.hpp"

#include <cstdint>

namespace app {

/**
 * ???????????? ????????? tick() ?? main (10..50 ms).
 * ???????? ? continuous move; ?????? ? ???????, AWC, ???????, ????.
 */
class PtzOperator {
public:
    PtzOperator(pt::IDevice& pt, ccam::devices::ICameraDevice& camera);

    void tick(const JoystickAxes& joy, PanelSettings& panel);

    const pt::Position& lastPosition() const { return last_position_; }
    bool hasPosition() const { return has_position_; }

private:
    pt::IDevice& pt_;
    ccam::devices::ICameraDevice& camera_;

    pt::Position last_position_{};
    bool has_position_ = false;
    bool pt_power_on_ = false;

    void applyJoystick(const JoystickAxes& joy, const PanelSettings& panel);
    void applyTouchAction(TouchAction action, PanelSettings& panel);
};

} // namespace app
