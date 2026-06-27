#pragma once

/**
 * Маски panel **status** (NIS §7 Table 1, `0x00`…`0x24`) для `Transaction::awaiting_status`.
 * `AppError` (`0xFB`) — только MCU, в маску не входит. См. также `bkcmd` (NIS §6.13).
 */

#include "nexMessages.hpp"
#include "nexTypes.hpp"

namespace nex {
namespace msg {

constexpr Status::Mask kAwaitingNone = 0ull;

/** NIS §7 п.23 — Serial Buffer Overflow, не зависит от bkcmd. */
constexpr Status::Mask kAwaitingBkcmdIndependent = Status::maskBit(Status::Code::Serial_Overflow);

/** Все именованные NIS Table 1 status в `Status::Code` (без AppError / Unrecognized_Header). */
constexpr Status::Mask kAwaitingAllPanel =
    Status::maskBit(Status::Code::Invalid_Instruction)
    | Status::maskBit(Status::Code::Success)
    | Status::maskBit(Status::Code::Invalid_CompId)
    | Status::maskBit(Status::Code::Invalid_PageId)
    | Status::maskBit(Status::Code::Invalid_PicId)
    | Status::maskBit(Status::Code::Invalid_FontId)
    | Status::maskBit(Status::Code::Invalid_FileOperation)
    | Status::maskBit(Status::Code::Invalid_CRC)
    | Status::maskBit(Status::Code::Invalid_BaudRate)
    | Status::maskBit(Status::Code::Invalid_Waveform_ID_Channel)
    | Status::maskBit(Status::Code::Invalid_VarName_Attr)
    | Status::maskBit(Status::Code::Invalid_VarOperation)
    | Status::maskBit(Status::Code::Failed_Assignment)
    | Status::maskBit(Status::Code::Failed_Eeprom)
    | Status::maskBit(Status::Code::Invalid_QuantityOfParameters)
    | Status::maskBit(Status::Code::Failed_IO_Operation)
    | Status::maskBit(Status::Code::Invalid_EscapeCharacter)
    | Status::maskBit(Status::Code::VarName_TooLong)
    | Status::maskBit(Status::Code::Serial_Overflow);

/** NIS §7 Table 1 success-байты (bkcmd 1|3). */
constexpr Status::Mask kAwaitingSuccessOnly = Status::maskBit(Status::Code::Success) | kAwaitingBkcmdIndependent;

/** NIS §7 Table 1 fail-байты (bkcmd 2|3). */
constexpr Status::Mask kAwaitingBkcmdOnFailure = kAwaitingAllPanel & ~Status::maskBit(Status::Code::Success);

/** `page` / `ref` / `sendme` — без file/IO status (фоновый шум DataRecord при смене страницы). */
constexpr Status::Mask kAwaitingPageCommand = kAwaitingAllPanel
    & ~Status::maskBit(Status::Code::Invalid_FileOperation)
    & ~Status::maskBit(Status::Code::Failed_IO_Operation);

/** Status шлётся панелью при данном bkcmd (Table 1 + bkcmd-independent). */
[[nodiscard]] constexpr Status::Mask bkcmdAllowedStatus(BkCmd bkcmd) noexcept {
    switch (bkcmd) 
    {
    case BkCmd::Off:       return kAwaitingBkcmdIndependent;
    case BkCmd::OnSuccess: return kAwaitingSuccessOnly;
    case BkCmd::OnFailure: return kAwaitingBkcmdOnFailure;
    case BkCmd::Always:    return kAwaitingAllPanel;
    }
    
    return kAwaitingAllPanel;
}

} // namespace msg
} // namespace nex
