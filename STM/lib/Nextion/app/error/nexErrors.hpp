#pragma once

/**
 * @file nexErrors.hpp
 *
 * Типы ошибок Nextion: reporter (`ErrorReporter`), операция (`AppOperation`/`AppStatus`),
 * деталь wire (`ErrorDetail`), политика очереди (`QueuePolicy`).
 * Обработка: `AppFailure` + `AppErrorHandler` (`app/error/nexAppErrorHandler.*`).
 */

#include <cstdint>
#include <cstdio>

#include "../../../Interfaces/ibyte_stream.hpp"
#include "../../../Interfaces/obj_registry.hpp"
#include "../../core/nexMessages.hpp"
#include "../../core/nexGateway.hpp"
#include "../../core/nexSession.hpp"

namespace nex {

/** Коды регистрации Page/Component (`Interfaces/obj_registry.hpp`, `MISC::RegStatus`). */
static_assert(static_cast<uint8_t>(MISC::RegStatus::SwappedWithOccupiedSlot) <= 0x0Fu,
    "RegStatus must fit in tag_2 low nibble");

enum class ErrorReporter : uint8_t {
    None = 0,
    Stream = 1,
    Gateway = 2,
    Session = 3,
    Command = 4,
    /** `MISC::RegStatus` — `ObjRegistry` / `ObjStorage` (Page, Component). */
    Domain = 5,
    Panel = 6,
};

using ErrorSubsystem = ErrorReporter;

enum class AppOperation : uint8_t {
    OK = 0,
    GatewayPushFailed,
    GatewayTransmitFailed,
    GatewayReceiveFailed,
    EnqueueRejected,
    SessionActivateFailed,
    SessionTimeout,
    PageRegisterFailed,
    ComponentRegisterFailed,
};

using AppStatus = AppOperation;

enum class AppProcedure : uint8_t {
    Runtime = 0,
    CompIdMapDiscover = 1,
};

enum class QueuePolicy : uint8_t {
    None,
    Notify,
    NotifyOptional,
    ResetActive,
    DropHead,
};

struct ErrorDetail {
    ErrorReporter reporter = ErrorReporter::None;
    uint8_t code = 0u;
};

inline constexpr uint8_t packDetail(ErrorReporter reporter, uint8_t code) noexcept {
    return static_cast<uint8_t>((static_cast<uint8_t>(reporter) << 4) | (code & 0x0Fu));
}

inline constexpr ErrorDetail unpackDetail(uint8_t tag2) noexcept {
    return ErrorDetail{static_cast<ErrorReporter>((tag2 >> 4) & 0x0Fu), static_cast<uint8_t>(tag2 & 0x0Fu)};
}

inline ErrorDetail detailFrom(BIF::IByteStream::Status st) noexcept {
    return ErrorDetail{ErrorReporter::Stream, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(Gateway::Status st) noexcept {
    return ErrorDetail{ErrorReporter::Gateway, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(Session::Status st) noexcept {
    return ErrorDetail{ErrorReporter::Session, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(Command::Status st) noexcept {
    return ErrorDetail{ErrorReporter::Command, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(MISC::RegStatus st) noexcept {
    return ErrorDetail{ErrorReporter::Domain, static_cast<uint8_t>(st)};
}

struct NexErrors {
    BIF::IByteStream::Status stream = BIF::IByteStream::Status::OK;
    Gateway::Status gateway = Gateway::Status::OK;
    Session::Status session = Session::Status::OK;

    void clear() noexcept {
        stream = BIF::IByteStream::Status::OK;
        gateway = Gateway::Status::OK;
        session = Session::Status::OK;
    }

    [[nodiscard]] bool any() const noexcept {
        return stream != BIF::IByteStream::Status::OK || gateway != Gateway::Status::OK
            || session != Session::Status::OK;
    }
};

static constexpr uint8_t kDefaultResetActiveRetryLimit = 8u;

[[nodiscard]] QueuePolicy defaultQueuePolicy(ErrorDetail cause) noexcept;

inline const char* byteStreamStatusCstr(BIF::IByteStream::Status st) noexcept {
    switch (st) {
    case BIF::IByteStream::Status::OK: return "OK";
    case BIF::IByteStream::Status::OverFlowRX: return "OverFlowRX";
    case BIF::IByteStream::Status::OverFlowTX: return "OverFlowTX";
    case BIF::IByteStream::Status::BitError: return "BitError";
    case BIF::IByteStream::Status::FrameError: return "FrameError";
    case BIF::IByteStream::Status::Disconnected: return "Disconnected";
    default: return "?";
    }
}

inline const char* errorDetailCstr(ErrorDetail d) noexcept {
    switch (d.reporter) {
    case ErrorReporter::Stream: return byteStreamStatusCstr(static_cast<BIF::IByteStream::Status>(d.code));
    case ErrorReporter::Gateway: return gatewayStatusCstr(static_cast<Gateway::Status>(d.code));
    case ErrorReporter::Session: return sessionStatusCstr(static_cast<Session::Status>(d.code));
    case ErrorReporter::Command: return commandStatusCstr(static_cast<Command::Status>(d.code));
    case ErrorReporter::Domain: return MISC::toStr(static_cast<MISC::RegStatus>(d.code));
    default: return "None";
    }
}

inline const char* appStatusCstr(AppStatus s) noexcept {
    switch (s) {
    case AppStatus::OK: return "OK";
    case AppStatus::GatewayPushFailed: return "GatewayPushFailed";
    case AppStatus::GatewayTransmitFailed: return "GatewayTransmitFailed";
    case AppStatus::GatewayReceiveFailed: return "GatewayReceiveFailed";
    case AppStatus::EnqueueRejected: return "EnqueueRejected";
    case AppStatus::SessionActivateFailed: return "SessionActivateFailed";
    case AppStatus::SessionTimeout: return "SessionTimeout";
    case AppStatus::PageRegisterFailed: return "PageRegisterFailed";
    case AppStatus::ComponentRegisterFailed: return "ComponentRegisterFailed";
    default: return "?";
    }
}

inline msg::Status makeAppError(AppStatus appStatus, ErrorDetail detail = {}) noexcept {
    msg::Status st{};
    st.status = msg::Status::Code::AppError;
    st.tag_1 = static_cast<uint8_t>(appStatus);
    st.tag_2 = (detail.reporter != ErrorReporter::None) ? packDetail(detail.reporter, detail.code) : 0u;
    return st;
}

inline const char* statusCodeCstr(msg::Status::Code c) noexcept {
    switch (c) {
    case msg::Status::Code::Invalid_Instruction: return "Invalid_Instruction";
    case msg::Status::Code::Success: return "Success";
    case msg::Status::Code::Invalid_CompId: return "Invalid_CompId";
    case msg::Status::Code::Invalid_PageId: return "Invalid_PageId";
    case msg::Status::Code::Invalid_PicId: return "Invalid_PicId";
    case msg::Status::Code::Invalid_FontId: return "Invalid_FontId";
    case msg::Status::Code::Invalid_FileOperation: return "Invalid_FileOperation";
    case msg::Status::Code::Invalid_CRC: return "Invalid_CRC";
    case msg::Status::Code::Invalid_BaudRate: return "Invalid_BaudRate";
    case msg::Status::Code::Invalid_Waveform_ID_Channel: return "Invalid_Waveform_ID_Channel";
    case msg::Status::Code::Invalid_VarName_Attr: return "Invalid_VarName_Attr";
    case msg::Status::Code::Invalid_VarOperation: return "Invalid_VarOperation";
    case msg::Status::Code::Failed_Assignment: return "Failed_Assignment";
    case msg::Status::Code::Failed_Eeprom: return "Failed_Eeprom";
    case msg::Status::Code::Invalid_QuantityOfParameters: return "Invalid_QuantityOfParameters";
    case msg::Status::Code::Failed_IO_Operation: return "Failed_IO_Operation";
    case msg::Status::Code::Invalid_EscapeCharacter: return "Invalid_EscapeCharacter";
    case msg::Status::Code::VarName_TooLong: return "VarName_TooLong";
    case msg::Status::Code::Serial_Overflow: return "Serial_Overflow";
    case msg::Status::Code::AppError: return "AppError";
    case msg::Status::Code::Unrecognized_Header: return "Unrecognized_Header";
    default: return "?";
    }
}

} // namespace nex
