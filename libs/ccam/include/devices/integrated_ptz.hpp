/**
 * @file integrated_ptz.hpp
 * @brief IntegratedPtzBase — встроенный P/T PTZ (HE/UE/HR): #APC/#APS/#PE/#LC.
 *
 * ConvertibleProtocol: эти команды есть у интегрированных камер;
 * у отдельных головок PH350 в матрице часто ---.
 */

#pragma once

#include "devices/pt_device.hpp"

namespace ccam::devices {

/**
 * Промежуточная база для PTZ со встроенным pan/tilt.
 * Общее continuous/presets/optics — в PtDeviceBase; абсолют и #LC/#PE — здесь.
 */
class IntegratedPtzBase : public PtDeviceBase {
public:
    explicit IntegratedPtzBase(Transport& transport)
        : PtDeviceBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::IntegratedPtz; }

    /** #APC — абсолютный pan/tilt. */
    Status setPT(const pt::PT& pt) override;
    /** Home = центр #APC. */
    Status goHome() override;

    /** #APS — абсолют + скорость. */
    Status setPTwSpeed(
        uint16_t pan,
        uint16_t tilt,
        uint8_t preset_speed,
        PrSpeed speed_table);
    /** #RPC — относительный pan/tilt. */
    Status relativePosition(uint16_t pan, uint16_t tilt);
    /** #RPS — относительный + скорость. */
    Status relPTwSpeed(
        uint16_t pan,
        uint16_t tilt,
        uint8_t preset_speed,
        PrSpeed speed_table);

    /**
     * #PE[bank] — карта занятых пресетов.
     * bank 0→1..40, 1→41..80, 2→81..100.
     */
    Status requestPresetEntryBank(uint8_t bank_0_to_2, uint64_t& bits40);
    /** Три #PE → occupied[0..99] (индекс = номер−1). */
    Status requestPresetMap(bool occupied[100]);

    /** #LC[axis][0|1] — явный Release/Set лимита. */
    Status setLimitationControl(LimitAxis axis, Limitation action);
    /** #LC[axis] — запрос состояния лимита. */
    Status requestLimitation(LimitAxis axis, Limitation& state);
};

} // namespace ccam::devices
