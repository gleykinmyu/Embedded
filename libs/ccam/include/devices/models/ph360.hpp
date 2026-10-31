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
    explicit PhHeadBase(Transport& transport)
        : PtDeviceBase(transport)
    {
    }

    /** Off→#O0, On→#O1; CamTX-режимы Param (на живых PH не работают). */
    Status power(pt::PowerMode mode) override
    {
        switch (mode) {
        case pt::PowerMode::Off:
            return sendDec1(catalog::PtCmd::Power, 0);
        case pt::PowerMode::On:
            return sendDec1(catalog::PtCmd::Power, 1);
        default:
            return Status::Param;
        }
    }

    /** Запрос состояния обогрева (#HS). */
    Status queryHeaterStatus(uint8_t& status) override
    {
        constexpr auto cmd = catalog::PtCmd::HeaterStatus;
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
};

/** AW-PH360 (+ RP400/IF400). */
class Ph360Pt : public PhHeadBase {
public:
    explicit Ph360Pt(Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph360; }
    const char* modelLabel() const override { return "AW-PH360"; }
};

/** AW-PH650. */
class Ph650Pt : public PhHeadBase {
public:
    explicit Ph650Pt(Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph650; }
    const char* modelLabel() const override { return "AW-PH650"; }
};

/** AW-PH405. */
class Ph405Pt : public PhHeadBase {
public:
    explicit Ph405Pt(Transport& transport)
        : PhHeadBase(transport)
    {
    }

    PtModelId modelId() const override { return PtModelId::Ph405; }
    const char* modelLabel() const override { return "AW-PH405"; }
};

} // namespace ccam::devices
