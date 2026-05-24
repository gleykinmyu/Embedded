/**
 * @file ptz_operator.hpp
 * @brief Связка джойстика + сенсорной панели с ccam::devices::IPtDevice / ICameraDevice.
 */

#pragma once

#include "app/joystick.hpp"
#include "app/touch_panel.hpp"
#include "devices/camera_device.hpp"
#include "devices/pt_device.hpp"

#include <cstdint>

namespace app {

/**
 * Периодически вызывайте tick() из main (10..50 ms).
 * Джойстик → непрерывные команды #P/#T/#Z/#F или #PTS.
 * Панель → пресеты, AWC, питание, стоп.
 */
class PtzOperator {
public:
    PtzOperator(ccam::devices::IPtDevice& pt, ccam::devices::ICameraDevice& camera);

    void tick(const JoystickAxes& joy, PanelSettings& panel);

    const ccam::PtPosition& lastPosition() const { return last_position_; }
    bool hasPosition() const { return has_position_; }

private:
    ccam::devices::IPtDevice& pt_;
    ccam::devices::ICameraDevice& camera_;

    ccam::PtPosition last_position_{};
    bool has_position_ = false;
    bool pt_power_on_ = false;

    void applyJoystick(const JoystickAxes& joy, const PanelSettings& panel);
    void applyTouchAction(TouchAction action, PanelSettings& panel);
};

} // namespace app
