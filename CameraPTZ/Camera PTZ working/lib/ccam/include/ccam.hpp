/**
 * @file ccam.hpp
 * @brief Panasonic Convertible Protocol — единая точка подключения.
 *
 * @code
 * #include "ccam.hpp"
 * #include "ibyte_stream.hpp"
 *
 * class MyUart : public BIF::IByteStream { ... };
 *
 * MyUart uart;
 * uart.open(ccam::kBaudRate);
 * ccam::Rs485Transport bus(uart, &myNowMs);
 * ccam::devices::He130Camera camera(bus);
 * ccam::devices::He130Pt pt(bus);
 * @endcode
 */

#pragma once

#include "catalog/catalog.hpp"
#include "devices/devices.hpp"
#include "protocol/camera_protocol.hpp"
#include "protocol/protocol_encoding.hpp"
#include "protocol/pt_types.hpp"
#include "transport/frame.hpp"
#include "transport/rs485_transport.hpp"
#include "transport/types.hpp"
