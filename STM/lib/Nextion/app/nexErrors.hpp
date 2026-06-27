#pragma once

#include <cstddef>
#include "../core/nexCommands.hpp"
#include "../core/nexDebug.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexSession.hpp"
#include "../../Interfaces/ibyte_stream.hpp"
#include "../../Interfaces/obj_registry.hpp"

namespace nex {

/** Синтетический `msg::Status::AppError`: упаковка ошибок MCU (`tag_1` / `tag_2`). */

namespace detail {

using NexLogTickFn = uint32_t (*)() noexcept;

void setNexLogTickFn(NexLogTickFn fn) noexcept;

#if defined(NEX_DEBUG)
void nexLogPrint(const char* fmt, ...) noexcept;
#endif

} // namespace detail

/** Источник MCU-ошибки в `msg::Status::AppError` (`tag_1`). */
enum class AppError : uint8_t {
    Register = 1u,
    Command,
    Session,
    Gateway,
    Stream,
};

inline msg::Status makeAppError(AppError reporter, uint16_t detail) noexcept {
    msg::Status s{};
    s.status = msg::Status::Code::AppError;
    s.tag_1 = static_cast<uint8_t>(reporter);
    s.tag_2 = detail;
    return s;
}

inline msg::Status appErrorFrom(MISC::RegStatus st) noexcept {
    return makeAppError(AppError::Register, static_cast<uint16_t>(st));
}

inline msg::Status appErrorFrom(Command::Status st) noexcept {
    return makeAppError(AppError::Command, static_cast<uint16_t>(st));
}

inline msg::Status appErrorFrom(Session::Status st, Gateway::Status gw = Gateway::Status::OK) noexcept {
    uint16_t detail = static_cast<uint16_t>(st);
    if (gw != Gateway::Status::OK)
        detail = static_cast<uint16_t>(static_cast<uint16_t>(st) | (static_cast<uint16_t>(gw) << 8));
    return makeAppError(AppError::Session, detail);
}

inline msg::Status appErrorFrom(Gateway::Status st) noexcept {
    return makeAppError(AppError::Gateway, static_cast<uint16_t>(st));
}

/** `tag_2` — `BIF::IByteStream::Status` (`OK` / `OverFlowRX` / `DataError`). */
inline msg::Status appErrorFrom(BIF::IByteStream::Status st) noexcept {
    return makeAppError(AppError::Stream, static_cast<uint16_t>(st));
}

/** Gateway в приоритете; иначе Stream (после RX). */
inline msg::Status appErrorFrom(Gateway::Status gw, BIF::IByteStream::Status stream) noexcept {
    if (gw != Gateway::Status::OK)
        return appErrorFrom(gw);
    return appErrorFrom(stream);
}

inline const char* cstr(AppError r) noexcept {
    switch (r) {
    case AppError::Register: return "Register";
    case AppError::Command: return "Command";
    case AppError::Session: return "Session";
    case AppError::Gateway: return "Gateway";
    case AppError::Stream: return "Stream";
    default: return "?";
    }
}

inline AppError appErrorReporter(const msg::Status& st) noexcept {
    return static_cast<AppError>(st.tag_1);
}

inline uint16_t appErrorDetail(const msg::Status& st) noexcept {
    return st.tag_2;
}

inline const char* appErrorDetailCstr(AppError reporter, uint16_t detail) noexcept {
    switch (reporter) {
    case AppError::Register:
        return MISC::cstr(static_cast<MISC::RegStatus>(detail));
    case AppError::Command:
        return cstr(static_cast<Command::Status>(detail));
    case AppError::Session:
        return cstr(static_cast<Session::Status>(detail & 0xFFu));
    case AppError::Gateway:
        return cstr(static_cast<Gateway::Status>(detail));
    case AppError::Stream:
        return BIF::cstr(static_cast<BIF::IByteStream::Status>(detail));
    default:
        return "?";
    }
}

std::size_t formatStatusMessage(const msg::Status& st, Route route, char* buf, size_t cap) noexcept;

/** Лог в UART при `NEX_DEBUG`; иначе no-op. */
void printStatusError(const msg::Status& st, Route route = {}) noexcept;

} // namespace nex
