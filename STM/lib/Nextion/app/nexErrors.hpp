#pragma once

#include <cstddef>
#include <cstdio>
#include "../core/nexCommands.hpp"
#include "../core/nexDebug.hpp"
#include "../core/nexGateway.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexSession.hpp"
#include "../../Interfaces/ibyte_stream.hpp"
#include "../../Interfaces/obj_registry.hpp"

namespace nex {

/** Источник MCU-ошибки в `msg::Status::AppError` (`tag_1`). */
enum class AppErrorReporter : uint8_t {
    Register = 1u,
    Command,
    Session,
    Gateway,
    Stream,
};

inline msg::Status makeAppError(AppErrorReporter reporter, uint16_t detail) noexcept {
    msg::Status s{};
    s.status = msg::Status::Code::AppError;
    s.tag_1 = static_cast<uint8_t>(reporter);
    s.tag_2 = detail;
    return s;
}

inline msg::Status appErrorFrom(MISC::RegStatus st) noexcept {
    return makeAppError(AppErrorReporter::Register, static_cast<uint16_t>(st));
}

inline msg::Status appErrorFrom(Command::Status st) noexcept {
    return makeAppError(AppErrorReporter::Command, static_cast<uint16_t>(st));
}

inline msg::Status appErrorFrom(Session::Status st, Gateway::Status gw = Gateway::Status::OK) noexcept {
    uint16_t detail = static_cast<uint16_t>(st);
    if (gw != Gateway::Status::OK)
        detail = static_cast<uint16_t>(static_cast<uint16_t>(st) | (static_cast<uint16_t>(gw) << 8));
    return makeAppError(AppErrorReporter::Session, detail);
}

inline msg::Status appErrorFrom(Gateway::Status st) noexcept {
    return makeAppError(AppErrorReporter::Gateway, static_cast<uint16_t>(st));
}

inline msg::Status appErrorFrom(BIF::IByteStream::Status st) noexcept {
    return makeAppError(AppErrorReporter::Stream, static_cast<uint16_t>(st));
}

/** Gateway в приоритете; иначе Stream (после RX). */
inline msg::Status appErrorFrom(Gateway::Status gw, BIF::IByteStream::Status stream) noexcept {
    if (gw != Gateway::Status::OK)
        return appErrorFrom(gw);
    return appErrorFrom(stream);
}

inline const char* cstr(AppErrorReporter r) noexcept {
    switch (r) {
    case AppErrorReporter::Register: return "Register";
    case AppErrorReporter::Command: return "Command";
    case AppErrorReporter::Session: return "Session";
    case AppErrorReporter::Gateway: return "Gateway";
    case AppErrorReporter::Stream: return "Stream";
    default: return "?";
    }
}

inline bool isAppError(const msg::Status& st) noexcept {
    return st.status == msg::Status::Code::AppError;
}

inline AppErrorReporter appErrorReporter(const msg::Status& st) noexcept {
    return static_cast<AppErrorReporter>(st.tag_1);
}

inline uint16_t appErrorDetail(const msg::Status& st) noexcept {
    return st.tag_2;
}

inline const char* appErrorDetailCstr(AppErrorReporter reporter, uint16_t detail) noexcept {
    switch (reporter) {
    case AppErrorReporter::Register:
        return MISC::cstr(static_cast<MISC::RegStatus>(detail));
    case AppErrorReporter::Command:
        return cstr(static_cast<Command::Status>(detail));
    case AppErrorReporter::Session:
        return cstr(static_cast<Session::Status>(detail & 0xFFu));
    case AppErrorReporter::Gateway:
        return cstr(static_cast<Gateway::Status>(detail));
    case AppErrorReporter::Stream:
        return BIF::cstr(static_cast<BIF::IByteStream::Status>(detail));
    default:
        return "?";
    }
}

inline std::size_t formatStatusMessage(const msg::Status& st, uint8_t page_id, uint8_t comp_id, char* buf,
    std::size_t cap) noexcept {
    if (cap == 0u)
        return 0u;
    if (isAppError(st)) {
        const AppErrorReporter reporter = appErrorReporter(st);
        const uint16_t detail = appErrorDetail(st);
        if ((detail >> 8) != 0u && reporter == AppErrorReporter::Session)
            return static_cast<std::size_t>(std::snprintf(buf, cap, "AppError %s %s gw=%s p%u c%u", cstr(reporter),
                appErrorDetailCstr(reporter, detail), cstr(static_cast<Gateway::Status>(detail >> 8)),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id)));
        return static_cast<std::size_t>(std::snprintf(buf, cap, "AppError %s %s p%u c%u", cstr(reporter),
            appErrorDetailCstr(reporter, detail), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id)));
    }
    return static_cast<std::size_t>(std::snprintf(buf, cap, "%s t1=%u t2=%u p%u c%u", cstr(st.status),
        static_cast<unsigned>(st.tag_1), static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id),
        static_cast<unsigned>(comp_id)));
}

inline void printStatusError(const msg::Status& st, uint8_t page_id, uint8_t comp_id) noexcept {
#if defined(NEX_DEBUG)
    if (isAppError(st)) {
        const AppErrorReporter reporter = appErrorReporter(st);
        const uint16_t detail = appErrorDetail(st);
        if ((detail >> 8) != 0u && reporter == AppErrorReporter::Session) {
            NEX_DBG("[Nextion] onError AppError %s %s gw=%s page=%u comp=%u\n", cstr(reporter),
                appErrorDetailCstr(reporter, detail), cstr(static_cast<Gateway::Status>(detail >> 8)),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        } else {
            NEX_DBG("[Nextion] onError AppError %s %s page=%u comp=%u\n", cstr(reporter),
                appErrorDetailCstr(reporter, detail), static_cast<unsigned>(page_id),
                static_cast<unsigned>(comp_id));
        }
    } else {
        NEX_DBG("[Nextion] onError %s (0x%02X) tag_1=%u tag_2=%u page=%u comp=%u\n", cstr(st.status),
            static_cast<unsigned>(static_cast<uint8_t>(st.status)), static_cast<unsigned>(st.tag_1),
            static_cast<unsigned>(st.tag_2), static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
    }
#else
    (void)st;
    (void)page_id;
    (void)comp_id;
#endif
}

} // namespace nex
