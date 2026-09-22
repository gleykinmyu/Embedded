/**
 * @file message_test.hpp
 * @brief Черновик: PDU как класс с базой протокола. Не подключено к Session/Node.
 *
 * Pdu            — id / cstr / pack / unpack (через базовый указатель).
 * ProtoPdu       — CRTP: в конструкторе static_assert диапазона; id()/cstr() из kId/kName.
 * TransportPdu   — служебные id: 0x00…0x0F и Heartbeat 0x30.
 * ConsolePdu     — northbound 0x10…0x4F.
 *
 * Цена виртуальности: vptr на экземпляр.
 */

#pragma once

#include <cstdint>

#include "smcp/transport/message.hpp"

namespace smcp {
namespace msgtest {

class Pdu {
public:
    virtual ~Pdu() = default;
    virtual uint8_t id() const noexcept = 0;
    virtual const char* cstr() const noexcept = 0;
    [[nodiscard]] virtual bool pack(msg::Message& m) const noexcept = 0;
    [[nodiscard]] virtual bool unpack(const msg::Message& m) noexcept = 0;
};

struct TransportProto {
    static constexpr uint8_t kIdLo = 0x00u;
    static constexpr uint8_t kIdHi = 0x0Fu;
    static constexpr uint8_t kHeartbeatId = 0x30u;

    [[nodiscard]] static constexpr bool owns(uint8_t id) noexcept
    {
        return (id >= kIdLo && id <= kIdHi) || id == kHeartbeatId;
    }
};

struct ConsoleProto {
    static constexpr uint8_t kIdLo = 0x10u;
    static constexpr uint8_t kIdHi = 0x4Fu;

    [[nodiscard]] static constexpr bool owns(uint8_t id) noexcept
    {
        return id >= kIdLo && id <= kIdHi;
    }
};

template <typename Derived, typename Proto>
class ProtoPdu : public Pdu {
public:
    [[nodiscard]] uint8_t id() const noexcept override { return Derived::kId; }
    [[nodiscard]] const char* cstr() const noexcept override { return Derived::kName; }

protected:
    ProtoPdu() noexcept
    {
        static_assert(Proto::owns(Derived::kId), "PDU id outside protocol range");
    }
};

template <typename Derived>
using TransportPdu = ProtoPdu<Derived, TransportProto>;

template <typename Derived>
using ConsolePdu = ProtoPdu<Derived, ConsoleProto>;

struct Ack final : TransportPdu<Ack> {
    static constexpr uint8_t kId = static_cast<uint8_t>(msg::TMsgId::Ack);
    static constexpr const char* kName = "Ack";
    static constexpr bool kNeedsAck = false;

    [[nodiscard]] bool pack(msg::Message& m) const noexcept override
    {
        m = {};
        m.id = kId;
        return true;
    }

    [[nodiscard]] bool unpack(const msg::Message& m) noexcept override
    {
        return m.id == kId && m.dlc == 0;
    }
};

struct Nack final : TransportPdu<Nack> {
    static constexpr uint8_t kId = static_cast<uint8_t>(msg::TMsgId::Nack);
    static constexpr const char* kName = "Nack";
    static constexpr bool kNeedsAck = false;
    static constexpr uint8_t kTimeout = 0x08u;
    static constexpr uint8_t kDetailNone = 0xFFu;

    uint8_t error = 0;
    uint8_t detail = kDetailNone;

    Nack() noexcept = default;
    Nack(uint8_t err, uint8_t det) noexcept
        : error(err)
        , detail(det)
    {}

    [[nodiscard]] bool pack(msg::Message& m) const noexcept override
    {
        m = {};
        m.id = kId;
        return m.push(error) && m.push(detail);
    }

    [[nodiscard]] bool unpack(const msg::Message& m) noexcept override
    {
        if (m.id != kId || m.dlc != 2) {
            return false;
        }
        error = m.data[0];
        detail = m.data[1];
        return true;
    }
};

struct Heartbeat final : TransportPdu<Heartbeat> {
    static constexpr uint8_t kId = static_cast<uint8_t>(msg::TMsgId::Heartbeat);
    static constexpr const char* kName = "Heartbeat";
    static constexpr bool kNeedsAck = false;

    [[nodiscard]] bool pack(msg::Message& m) const noexcept override
    {
        m = {};
        m.id = kId;
        return true;
    }

    [[nodiscard]] bool unpack(const msg::Message& m) noexcept override
    {
        return m.id == kId && m.dlc == 0;
    }
};

struct Select final : ConsolePdu<Select> {
    static constexpr uint8_t kId = 0x10u;
    static constexpr const char* kName = "Select";
    static constexpr bool kNeedsAck = true;
    static constexpr uint8_t kActionMax = 2u;

    uint8_t action = 0;
    uint32_t mask = 0;

    Select() noexcept = default;
    Select(uint8_t act, uint32_t selection) noexcept
        : action(act)
        , mask(selection)
    {}

    [[nodiscard]] bool pack(msg::Message& m) const noexcept override
    {
        m = {};
        m.id = kId;
        return m.push(action) && m.pushU32(mask);
    }

    [[nodiscard]] bool unpack(const msg::Message& m) noexcept override
    {
        if (m.id != kId || m.dlc != 5) {
            return false;
        }
        action = m.data[0];
        mask = m.loadU32(1);
        return action <= kActionMax;
    }
};

[[nodiscard]] bool selfCheck() noexcept;

} // namespace msgtest
} // namespace smcp
