/**
 * @file nack_fmt.hpp
 * @brief Человекочитаемая расшифровка session::Nack (+ detail session::MultiByte / app).
 */

#pragma once

#include <cstdio>
#include <cstdint>

#include "smcp/message.hpp"
#include "segment/message.hpp"
#include "smcp/session/multi_byte.hpp"

namespace smcp {
namespace host {

[[nodiscard]] inline const char* msgIdName(uint8_t id) noexcept
{
    switch (id) {
    case session::Ack::kId: return "Ack";
    case session::Nack::kId: return "Nack";
    case session::Fault::kId: return "Fault";
    case session::Heartbeat::kId: return "HB";
    case session::Get::kId: return "Get";
    case session::Set::kId: return "Set";
    case session::DataBegin::kId: return "DataBegin";
    case session::Data::kId: return "Data";
    default: return nullptr;
    }
}

/**
 * @return длина записанной строки (без \\0).
 * Пример: req=Set txn=2 err=Body(3) mb=Crc detail=5
 */
inline int formatNack(char* buf, std::size_t n, uint8_t req_msg_id, uint8_t txn_id,
                      const session::Nack& reply) noexcept
{
    char req_buf[16]{};
    if (const char* rn = msgIdName(req_msg_id)) {
        std::snprintf(req_buf, sizeof(req_buf), "%s", rn);
    } else {
        std::snprintf(req_buf, sizeof(req_buf), "0x%02X", static_cast<unsigned>(req_msg_id));
    }

    const char* err_name = segment::vError::from_raw(reply.error.raw()).cstr();

    if (reply.error == session::Error::Body) {
        const auto mb = static_cast<session::MultiByte::Error>(static_cast<uint8_t>(reply.detail));
        return std::snprintf(buf, n,
            "req=%s txn=%u err=%s(%u) mb=%s detail=%u",
            req_buf,
            static_cast<unsigned>(txn_id),
            err_name,
            static_cast<unsigned>(static_cast<uint8_t>(reply.error)),
            session::MultiByte::cstr(mb),
            static_cast<unsigned>(reply.detail));
    }

    if (reply.detail != session::Nack::kDetailNone) {
        return std::snprintf(buf, n,
            "req=%s txn=%u err=%s(%u) detail=0x%08X",
            req_buf,
            static_cast<unsigned>(txn_id),
            err_name,
            static_cast<unsigned>(static_cast<uint8_t>(reply.error)),
            static_cast<unsigned>(reply.detail));
    }

    return std::snprintf(buf, n,
        "req=%s txn=%u err=%s(%u)",
        req_buf,
        static_cast<unsigned>(txn_id),
        err_name,
        static_cast<unsigned>(static_cast<uint8_t>(reply.error)));
}

} // namespace host
} // namespace smcp
