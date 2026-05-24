/**
 * @file ccam.hpp
 * @brief Panasonic Convertible Protocol — единая точка подключения.
 *
 * @code
 * #include "ccam.hpp"
 *
 * class MyHal : public ccam::IRs485Hal { ... };
 *
 * MyHal hal;
 * ccam::Rs485Transport bus(hal);
 * ccam::devices::He130Camera camera(bus);
 * ccam::devices::He130Pt pt(bus);
 * @endcode
 */

#pragma once

#include "protocol/camera_protocol.hpp"
#include "catalog/catalog.hpp"
#include "devices/devices.hpp"
#include "transport/frame.hpp"
#include "protocol/protocol_encoding.hpp"
#include "protocol/pt_types.hpp"
#include "transport/rs485_hal.hpp"
#include "transport/rs485_transport.hpp"
#include "transport/types.hpp"
