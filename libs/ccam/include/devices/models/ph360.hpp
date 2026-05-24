/**
 * @file ph360.hpp
 * @brief AW-PH360 / PH650 / PH405 — поворотная головка (отдельно от камеры).
 *
 * ConvertibleProtocol.pdf v3.05, раздел P/T, стр. 29–32.
 * Камера подключается отдельно; здесь только #-протокол головки.
 */

#pragma once

#include "devices/pt_device.hpp"
#include "protocol/protocol_encoding.hpp"

#include <cstdint>

namespace ccam::devices {

/** Базовый класс головки серии PH (общие опции: wiper, fan, tally). */
class PhHeadBase : public PtDeviceBase {
public:
    explicit PhHeadBase(Rs485Transport& transport)
        : PtDeviceBase(transport)
    {
    }

    Status enableTally(bool on)
    {
        return setTallyEnable(on);
    }

    Status runWiper(uint8_t mode)
    {
        return setWiper(mode);
    }

    Status runWasher(uint8_t mode)
    {
        return setWasher(mode);
    }

    Status setFanMode(uint8_t mode)
    {
        return setFan(mode);
    }

    /** Запрос состояния обогрева (#HS). */
    Status queryHeaterStatus(uint8_t& status)
    {
        uint8_t response[8];
        size_t len = 0;
        Status st = transact(
            catalog::PtCmd::HeaterStatus, nullptr, response, sizeof(response), &len);
        if (st != Status::Ok || len < 2) {
            return Status::Io;
        }
        return decodeDec1ResponseByte(response[1], status);
    }
};

/** AW-PH360 (+ RP400/IF400). */
class Ph360Pt : public PhHeadBase {
public:
    explicit Ph360Pt(Rs485Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph360; }
    const char* modelLabel() const override { return "AW-PH360"; }
};

/** AW-PH650. */
class Ph650Pt : public PhHeadBase {
public:
    explicit Ph650Pt(Rs485Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph650; }
    const char* modelLabel() const override { return "AW-PH650"; }
};

/** AW-PH405. */
class Ph405Pt : public PhHeadBase {
public:
    explicit Ph405Pt(Rs485Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph405; }
    const char* modelLabel() const override { return "AW-PH405"; }
};

} // namespace ccam::devices
