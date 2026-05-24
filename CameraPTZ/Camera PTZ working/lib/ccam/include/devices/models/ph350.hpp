/**
 * @file ph350.hpp
 * @brief AW-PH350 / AW-PH350E — поворотная головка.
 *
 * ConvertibleProtocol.pdf v3.05, колонка PH350 (стр. 29–32).
 * Камера (E600/E650 и др.) подключается отдельно по camera protocol.
 */

#pragma once

#include "devices/models/ph360.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/**
 * AW-PH350E — головка серии PH350.
 *
 * Поддерживает power 0..3 (в т.ч. ON w/ Camera TX), pan/tilt/zoom/focus,
 * пресеты, wiper/fan/tally (через PhHeadBase).
 *
 * @code
 * ccam::devices::E650Camera camera(bus);
 * ccam::devices::Ph350Pt pt(bus);
 *
 * pt.power(ccam::PtPowerMode::OnWithCameraTx);
 * pt.recallPreset(1);
 * @endcode
 */
class Ph350Pt : public PhHeadBase {
public:
    static constexpr uint8_t kMaxPreset = 99;

    explicit Ph350Pt(Rs485Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph350; }
    const char* modelLabel() const override { return "AW-PH350E"; }

    /** Типичный режим: питание головки + проброс линии camera TX. */
    Status powerWithCameraLink()
    {
        return power(PtPowerMode::OnWithCameraTx);
    }

    Status goHome()
    {
        return absolutePosition(kPtPosCenter, kPtPosCenter);
    }

    /** Zoom position #AXZ (555h..FFFh). */
    Status setZoomPositionHex(uint16_t pos)
    {
        return setZoomPosition(pos);
    }

    /** Focus position #AXF. */
    Status setFocusPositionHex(uint16_t pos)
    {
        return setFocusPosition(pos);
    }

    Status queryDefrosterStatus(uint8_t& status)
    {
        uint8_t response[8];
        size_t len = 0;
        Status st = transact(
            catalog::PtCmd::DefrosterStatus, nullptr, response, sizeof(response), &len);
        if (st != Status::Ok || len < 2) {
            return Status::Io;
        }
        return decodeDec1ResponseByte(response[1], status);
    }
};

} // namespace ccam::devices
