#pragma once

#include <cstdint>
#include <cstdio>

#include "../../Interfaces/ibyte_stream.hpp"
#include "nexMessages.hpp"
#include "nexGateway.hpp"
#include "nexSession.hpp"

namespace nex {

/** Код в `tag_2` при `Application::Status::PageRegisterFailed` / `ComponentRegisterFailed` (подсистема Register). */
enum class RegisterError : uint8_t {
    PageIdOutOfRange = 0,     /**< `Page::ID` ≥ `kMaxPages`. */
    PageSlotOccupied,         /**< `_pages[id]` уже указывает на другую страницу. */
    PageIdNotSequential,      /**< `Page::ID` ≠ числу уже зарегистрированных страниц (ожидается 0, 1, 2, …). */
    ComponentNullPointer,     /**< `registerComponent(nullptr)` или `rebindComponentId(nullptr, …)`. */
    ComponentIdOutOfRange,    /**< `id()` ≥ размера таблицы `PageImpl`. */
    ComponentRegistryFull,    /**< автослот (`id==0` в конструкторе): нет места в `_registry[1…]`. */
};

enum class ErrorSubsystem : uint8_t {
    None = 0,
    Stream = 1,
    Gateway = 2,
    Session = 3,
    Command = 4,
    Register = 5,
};

struct ErrorDetail {
    ErrorSubsystem subsystem = ErrorSubsystem::None;
    uint8_t code = 0u;
};

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

/** Действие с очередью и уведомление `onError` после сбоя (`notifyAppError` в Application). */
enum class QueuePolicy : uint8_t {
    None,            /**< Без onError и без изменения очереди. */
    Notify,          /**< onError обязательно; очередь не менять. */
    NotifyOptional,  /**< onError при `Application::notifyOptional()`; очередь не менять. */
    ResetActive,     /**< onError обязательно; `applyQueueRecovery` — сброс active. */
    DropHead,        /**< onError обязательно; `applyQueueRecovery` — снять голову. */
};

inline constexpr uint8_t packDetail(ErrorSubsystem subsystem, uint8_t code) noexcept {
    return static_cast<uint8_t>((static_cast<uint8_t>(subsystem) << 4) | (code & 0x0Fu));
}

inline constexpr ErrorDetail unpackDetail(uint8_t tag2) noexcept {
    return ErrorDetail{static_cast<ErrorSubsystem>((tag2 >> 4) & 0x0Fu), static_cast<uint8_t>(tag2 & 0x0Fu)};
}

inline ErrorDetail detailFrom(BIF::IByteStream::Status st) noexcept {
    return ErrorDetail{ErrorSubsystem::Stream, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(Gateway::Status st) noexcept {
    return ErrorDetail{ErrorSubsystem::Gateway, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(Session::Status st) noexcept {
    return ErrorDetail{ErrorSubsystem::Session, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(Command::Status st) noexcept {
    return ErrorDetail{ErrorSubsystem::Command, static_cast<uint8_t>(st)};
}

inline ErrorDetail detailFrom(RegisterError e) noexcept {
    return ErrorDetail{ErrorSubsystem::Register, static_cast<uint8_t>(e)};
}

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

/** Строка для `errorDetailCstr` / `formatStatusMessage`. */
inline const char* registerErrorCstr(RegisterError e) noexcept {
    switch (e) {
    case RegisterError::PageIdOutOfRange: return "PageIdOutOfRange";
    case RegisterError::PageSlotOccupied: return "PageSlotOccupied";
    case RegisterError::PageIdNotSequential: return "PageIdNotSequential";
    case RegisterError::ComponentNullPointer: return "ComponentNullPointer";
    case RegisterError::ComponentIdOutOfRange: return "ComponentIdOutOfRange";
    case RegisterError::ComponentRegistryFull: return "ComponentRegistryFull";
    default: return "?";
    }
}

inline const char* errorDetailCstr(ErrorDetail d) noexcept {
    switch (d.subsystem) {
    case ErrorSubsystem::Stream: return byteStreamStatusCstr(static_cast<BIF::IByteStream::Status>(d.code));
    case ErrorSubsystem::Gateway: return gatewayStatusCstr(static_cast<Gateway::Status>(d.code));
    case ErrorSubsystem::Session: return sessionStatusCstr(static_cast<Session::Status>(d.code));
    case ErrorSubsystem::Command: return commandStatusCstr(static_cast<Command::Status>(d.code));
    case ErrorSubsystem::Register: return registerErrorCstr(static_cast<RegisterError>(d.code));
    default: return "None";
    }
}

inline QueuePolicy defaultQueuePolicy(Gateway::Status s) noexcept {
    switch (s) {
    case Gateway::Status::TxBusy:
    case Gateway::Status::TxBusyRaw:
    case Gateway::Status::StreamTxError:
        return QueuePolicy::ResetActive;
    case Gateway::Status::SerializeFailed:
    case Gateway::Status::EmptyPayload:
    case Gateway::Status::NullPointer:
        return QueuePolicy::DropHead;
    default:
        return QueuePolicy::Notify;
    }
}

// TODO(ByteStreamPolicy): разная реакция на ошибки UART (сейчас упрощённо DropHead / ResetActive).
// — Disconnected: нет связи с панелью → остановить Application (не крутить очередь/update), onError +
//   режим «link down» до восстановления кабеля/питания Nextion; не DropHead как единственная мера.
// — BitError / FrameError: линия жива, но кадр битый → попытки восстановления (смена baud, re-init UART,
//   flush RX, повтор с лимитом) перед эскалацией; onError с подсказкой, что проверить скорость/проводку.
// — OverFlowTX/RX + ResetActive: TODO(TransmitRetryLimit) — счётчик повторов на голову, затем DropHead
//   или отдельный Application::Status; см. applyQueueRecovery в nexApplicationErrors.cpp.
/** Политика для UART (временная): retry через `ResetActive` или `DropHead` + `onError`. */
inline QueuePolicy defaultQueuePolicy(BIF::IByteStream::Status st) noexcept {
    switch (st) {
    case BIF::IByteStream::Status::OK:
        return QueuePolicy::None;
    case BIF::IByteStream::Status::OverFlowTX:
    case BIF::IByteStream::Status::OverFlowRX:
        return QueuePolicy::ResetActive;
    case BIF::IByteStream::Status::Disconnected:
    case BIF::IByteStream::Status::BitError:
    case BIF::IByteStream::Status::FrameError:
        return QueuePolicy::DropHead;
    default:
        return QueuePolicy::Notify;
    }
}

inline QueuePolicy defaultQueuePolicy(Session::Status st) noexcept {
    switch (st) {
    case Session::Status::NotIdle:
    case Session::Status::QueueEmpty:
        return QueuePolicy::NotifyOptional;
    default:
        return QueuePolicy::Notify;
    }
}

// Ошибки сборки команды (null, пустые поля, слот очереди, …) → DropHead.
// TxFrameOverflow и прочие не перечисленные — default → Notify (повтор/ручная обработка без снятия головы).
inline QueuePolicy defaultQueuePolicy(Command::Status st) noexcept {
    switch (st) {
    case Command::Status::NullPointer:
    case Command::Status::EmptyLiteral:
    case Command::Status::EmptyAttribute:
    case Command::Status::InvalidColor:
    case Command::Status::InvalidGeometry:
    case Command::Status::InvalidFields:
    case Command::Status::UnknownKind:
    case Command::Status::SlotTooSmall:
    case Command::Status::SlotAlignMismatch:
        return QueuePolicy::DropHead;
    default:
        return QueuePolicy::Notify;
    }
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
