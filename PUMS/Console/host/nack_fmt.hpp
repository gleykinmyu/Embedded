/**
 * @file nack_fmt.hpp
 * @brief Человекочитаемая расшифровка msg::Nack (+ detail MultiByte / app).
 */

#pragma once

#include <cstdio>
#include <cstdint>

#include "smcp/message.hpp"
#include "smcp/session/multi_byte.hpp"

namespace smcp {
namespace host {

[[nodiscard]] inline const char* msgIdName(uint8_t id) noexcept
{
    switch (id) {
    case msg::Ack::kId: return "Ack";
    case msg::Nack::kId: return "Nack";
    case msg::Fault::kId: return "Fault";
    case msg::Heartbeat::kId: return "HB";
    case msg::Get::kId: return "Get";
    case msg::Set::kId: return "Set";
    case msg::DataBegin::kId: return "DataBegin";
    case msg::Data::kId: return "Data";
    default: return nullptr;
    }
}

/** Транспорт (≤ Max) + известные прикладные (SYSTEM / mech ErrorCode). */
[[nodiscard]] inline const char* nackErrorName(uint8_t err) noexcept
{
    if (err == msg::Nack::kTimeout) {
        return "Timeout"; /* локальный abort окна Ack */
    }
    if (err <= static_cast<uint8_t>(msg::Nack::Error::Max)) {
        switch (static_cast<msg::Nack::Error>(err)) {
        case msg::Nack::Error::None: return "None";
        case msg::Nack::Error::NotSupported: return "NotSupported";
        case msg::Nack::Error::Busy: return "Busy";
        case msg::Nack::Error::Body: return "Body";
        case msg::Nack::Error::Max: return "Max";
        }
    }
    switch (err) {
    case 0x10: return "AppBusy";
    case 0x11: return "Limits";
    case 0x12: return "Crc";
    case 0x13: return "MechNotFound";
    case 0x14: return "Safety";
    case 0x15: return "NotReady";
    case 0x16: return "SelectLimit";
    default: break;
    }
    return "Unknown";
}

/**
 * @return длина записанной строки (без \\0).
 * Пример: req=Set txn=2 err=Body(3) mb=Crc detail=5
 */
inline int formatNack(char* buf, std::size_t n, uint8_t req_msg_id, uint8_t txn_id,
                      const msg::Nack& reply) noexcept
{
    char req_buf[16]{};
    if (const char* rn = msgIdName(req_msg_id)) {
        std::snprintf(req_buf, sizeof(req_buf), "%s", rn);
    } else {
        std::snprintf(req_buf, sizeof(req_buf), "0x%02X", static_cast<unsigned>(req_msg_id));
    }

    const char* err_name = nackErrorName(reply.error);

    if (reply.error == static_cast<uint8_t>(msg::Nack::Error::Body)) {
        const auto mb = static_cast<MultiByte::Error>(static_cast<uint8_t>(reply.detail));
        return std::snprintf(buf, n,
            "req=%s txn=%u err=%s(%u) mb=%s detail=%u",
            req_buf,
            static_cast<unsigned>(txn_id),
            err_name,
            static_cast<unsigned>(reply.error),
            MultiByte::cstr(mb),
            static_cast<unsigned>(reply.detail));
    }

    if (reply.detail != msg::Nack::kDetailNone) {
        return std::snprintf(buf, n,
            "req=%s txn=%u err=%s(%u) detail=0x%08X",
            req_buf,
            static_cast<unsigned>(txn_id),
            err_name,
            static_cast<unsigned>(reply.error),
            static_cast<unsigned>(reply.detail));
    }

    return std::snprintf(buf, n,
        "req=%s txn=%u err=%s(%u)",
        req_buf,
        static_cast<unsigned>(txn_id),
        err_name,
        static_cast<unsigned>(reply.error));
}

} // namespace host
} // namespace smcp
