/**
 * @file devices.hpp
 * @brief Иерархия устройств Convertible Protocol — единая точка подключения.
 *
 * @code
 * #include "devices/devices.hpp"
 *
 * ccam::Rs485Transport bus(hal);
 * ccam::devices::He130Camera camera(bus);
 * ccam::devices::He130Pt pt(bus);
 *
 * camera.setGain("08");
 * pt.recallPreset(1);
 * @endcode
 */

#pragma once

#include "devices/camera_device.hpp"
#include "devices/device_types.hpp"
#include "devices/protocol_values.hpp"
#include "devices/models/generic.hpp"
#include "devices/models/e600.hpp"
#include "devices/models/e650.hpp"
#include "devices/models/he130.hpp"
#include "devices/models/he870.hpp"
#include "devices/models/ph350.hpp"
#include "devices/models/ph360.hpp"
#include "devices/models/ue150.hpp"
#include "devices/pt_device.hpp"
