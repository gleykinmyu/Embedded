#pragma once
#include "config.h"
#include "phl.h"

namespace IRQ {

    // Базовый класс-якорь
    struct IHandlerBase {
        virtual ~IHandlerBase() = default;
    };

    // Реестр идеального размера (на основе ID::Count из phl.h)
    static inline IHandlerBase* Registry[PHL::PeriphCount] = {nullptr};

    // Шаблон интерфейса
    template<PHL::ID _id, PHL::Type _type = PHL::GetType<_id>::value>
    class IHandler;

    // --- UART (Один вектор) ---
    template<PHL::ID _id>
    class IHandler<_id, PHL::Type::UART> : public IHandlerBase {
        using Dev = PHL::IBase<_id>;
    public:
        IHandler() { Registry[static_cast<uint8_t>(Dev::index)] = this; }
        virtual void IrqHandler() = 0;
        ~IHandler() override { Registry[static_cast<uint8_t>(Dev::index)] = nullptr; }
    };

    // --- SPI (Один вектор) ---
    template<PHL::ID _id>
    class IHandler<_id, PHL::Type::SPI> : public IHandlerBase {
        using Dev = PHL::IBase<_id>;
    public:
        IHandler() { Registry[static_cast<uint8_t>(Dev::index)] = this; }
        virtual void IrqHandler() = 0;
        ~IHandler() override { Registry[static_cast<uint8_t>(Dev::index)] = nullptr; }
    };

    // --- I2C (Два вектора: Event и Error) ---
    template<PHL::ID _id>
    class IHandler<_id, PHL::Type::I2C> : public IHandlerBase {
        using Dev = PHL::IBase<_id>;
    public:
        IHandler() { Registry[static_cast<uint8_t>(Dev::index)] = this; }
        virtual void EventIrqHandler() = 0;
        virtual void ErrorIrqHandler() = 0;
        ~IHandler() override { Registry[static_cast<uint8_t>(Dev::index)] = nullptr; }
    };

    // --- CAN (Четыре вектора: TX, RX0, RX1, SCE) ---
    template<PHL::ID _id>
    class IHandler<_id, PHL::Type::CAN> : public IHandlerBase {
        using Dev = PHL::IBase<_id>;
    public:
        IHandler() { Registry[static_cast<uint8_t>(Dev::index)] = this; }
        virtual void TxIrqHandler() = 0;
        virtual void Rx0IrqHandler() = 0;
        virtual void Rx1IrqHandler() = 0;
        virtual void SceIrqHandler() = 0;
        ~IHandler() override { Registry[static_cast<uint8_t>(Dev::index)] = nullptr; }
    };

    // --- SDIO / SDMMC (Один вектор) ---
    template<PHL::ID _id>
    class IHandler<_id, PHL::Type::SDIO> : public IHandlerBase {
        using Dev = PHL::IBase<_id>;
    public:
        IHandler() { Registry[static_cast<uint8_t>(Dev::index)] = this; }
        virtual void IrqHandler() = 0;
        ~IHandler() override { Registry[static_cast<uint8_t>(Dev::index)] = nullptr; }
    };

} // namespace IRQ
