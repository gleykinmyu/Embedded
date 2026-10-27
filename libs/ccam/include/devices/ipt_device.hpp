/**
 * @file ipt_device.hpp
 * @brief Универсальный PTZ-интерфейс pt::IDevice и его типы.
 *
 * Контракт в духе Axis VAPIX / ONVIF: continuous move, stop, home, presets,
 * position. Без протокол-специфичной шкалы скоростей (это в PtDeviceBase).
 */

#pragma once

#include "devices/device_types.hpp"

#include <cstdint>

namespace pt {

/** Результат операций PT / транспорта. */
enum class Status {
    Ok = 0,
    Param,    ///< Неверный аргумент
    Io,       ///< Ошибка UART / HAL
    Timeout,  ///< Нет ответа в отведённое время
    Nack,     ///< Устройство не вернуло ACK
    Buffer,   ///< Переполнение буфера кадра
};

/** Режим питания головки / PTZ. */
enum class PowerMode : uint8_t {
    Off = 0,
    On = 1,
    OnWithCameraTx = 2,
    OnWithoutCameraTx = 3,
};

/** Ось непрерывного движения. */
enum class Axis : uint8_t {
    Pan,
    Tilt,
    Zoom,
    Focus,
    Iris,
    Roll,  ///< Вращение вокруг оптической оси (если есть на головке)
};

/** Абсолютная позиция pan/tilt. */
struct PT {
    uint16_t pan = 0;
    uint16_t tilt = 0;
};

/**
 * Абсолютная позиция оптики, шкала 0x0000..0xFFFF (min..max).
 * Внутри PtDeviceBase / CameraDeviceBase → натив протокола.
 */
struct Lens {
    uint16_t zoom = 0;   ///< Wide .. Tele
    uint16_t focus = 0;  ///< Near .. Far
    uint16_t iris = 0;   ///< Close .. Open (P/T AXI или cam ORV; не OIF F-number)
};

/** Pan/tilt + roll (если ось есть на головке). */
struct ExPT : PT {
    uint16_t roll = 0;
};

/** Позиция: головка + оптика. */
struct Position {
    PT pt{};
    Lens lens{};
};

/** Расширенная позиция: ExPT + оптика. */
struct ExPosition {
    ExPT pt{};
    Lens lens{};
};

/** Экспозиция: gain / shutter / ND. */
struct Exposure {
    uint16_t gain = 0;
    uint16_t shutter = 0;
    uint16_t nd = 0;
};

/**
 * Универсальный PTZ-интерфейс для UI / операторов / любых головок.
 * Реализация протокола — в наследнике (Panasonic: PtDeviceBase).
 */
class IDevice {
public:
    virtual ~IDevice() = default;

    virtual ccam::devices::PtModelId modelId() const = 0;
    virtual const char* modelLabel() const = 0;

    virtual Status power(PowerMode mode) = 0;

    /**
     * Непрерывное движение по оси.
     * speed: полный int8_t (−128..127); <0 / >0 — сторона, 0 — стоп оси.
     * Масштаб в протокол производителя — в реализации (PtDeviceBase и т.п.).
     * Pan −/+ = Left/Right, Tilt −/+ = Down/Up,
     * Zoom −/+ = Wide/Tele, Focus/Iris −/+ = Near/Far,
     * Roll −/+ = против/по часовой (если ось есть).
     */
    virtual Status move(Axis axis, int8_t speed) = 0;
    /** Одновременный pan+tilt (signed −128..127; 0 = стоп оси). */
    virtual Status movePT(int8_t pan, int8_t tilt)
    {
        Status st = move(Axis::Pan, pan);
        if (st != Status::Ok) {
            return st;
        }
        return move(Axis::Tilt, tilt);
    }
    /** Остановить непрерывное движение. */
    virtual Status stop() = 0;

    /** Абсолютная pan/tilt, единицы — у реализации. */
    virtual Status setPT(const PT& pt) = 0;
    /** Абсолютная оптика, единицы — у реализации. */
    virtual Status setLens(const Lens& lens) = 0;
    /** Полная абсолютная позиция (PT + lens). */
    virtual Status setPosition(const Position& position)
    {
        Status st = setPT(position.pt);
        if (st != Status::Ok) {
            return st;
        }
        return setLens(position.lens);
    }
    /** В домашнюю позицию (смысл home — у реализации). */
    virtual Status goHome() = 0;

    virtual Status recallPreset(uint8_t preset) = 0;
    virtual Status savePreset(uint8_t preset) = 0;
    virtual Status deletePreset(uint8_t preset) = 0;
    virtual Status requestPosition(Position& position) = 0;
};

} // namespace pt
