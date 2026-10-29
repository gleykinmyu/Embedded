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
 * pt.power(pt::PowerMode::OnWithCameraTx);
 * pt.recallPreset(1);
 * @endcode
 */
class Ph350Pt : public PhHeadBase {
public:
    static constexpr uint8_t kMaxPreset = 99;
    /** Пресет «дома» (ConvertibleProtocol: #APC у PH350 в матрице не поддержан). */
    static constexpr uint8_t kDefaultHomePreset = 1;

    explicit Ph350Pt(Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph350; }
    const char* modelLabel() const override { return "AW-PH350E"; }

    /**
     * Home = #Rnn (preset), не #APC80008000.
     * Сохраните центр через Save preset (по умолчанию #01), затем Home.
     */
    Status goHome() override { return recallPreset(home_preset_); }

    /**
     * PH350: #PTV/#PTD/#APC в матрице нет.
     * pan/tilt → центр (неизвестно); zoom/focus/iris ← #GZ/#GF/#GI.
     */
    Status requestPosition(pt::Position& position) override
    {
        position.pt.pan = kPtPosCenter;
        position.pt.tilt = kPtPosCenter;
        Status st_z = requestZoom(position.lens.zoom);
        Status st_f = requestFocus(position.lens.focus);
        Status st_i = requestIris(position.lens.iris);
        if (st_z != Status::Ok) {
            return st_z;
        }
        if (st_f != Status::Ok) {
            return st_f;
        }
        return st_i;
    }

    void setHomePreset(uint8_t preset_1_to_99)
    {
        if (preset_1_to_99 >= 1 && preset_1_to_99 <= kMaxPreset) {
            home_preset_ = preset_1_to_99;
        }
    }
    uint8_t homePreset() const { return home_preset_; }

    Status queryDefrosterStatus(uint8_t& status) override
    {
        constexpr auto cmd = catalog::PtCmd::DefrosterStatus;
        uint8_t response[8];
        size_t len = 0;
        Status st = transact(cmd, nullptr, response, sizeof(response), &len);
        if (st != Status::Ok) {
            return st;
        }
        size_t pos = 0;
        st = matchPtResponse(response, len, cmd, pos);
        if (st != Status::Ok || pos >= len) {
            return Status::Io;
        }
        return decodeDec1ResponseByte(response[pos], status);
    }

private:
    uint8_t home_preset_{kDefaultHomePreset};
};

} // namespace ccam::devices
