#pragma once

/**
 * @file nexStatusMask.hpp
 *
 * Маска приёмных panel-status (NIS §7 Table 1, wire 0x00…0x24) для `Transaction`.
 * `AppError (0xFB)` — только MCU, в маску не входит.
 *
 * NIS §6.13 `bkcmd`: Table 1 — Success при 1|3, fail при 2|3; Off — ничего из Table 1.
 * Не зависят от bkcmd: Serial Overflow (0x24, §7 п.23) и Table 2 (0x70/0x71, touch, …).
 *
 * `kAwaitingNone` (0) — NoAwaiting: session закрывается на txIdle.
 */

#include "nexMessages.hpp"
#include "nexTypes.hpp"

namespace nex {

/** Бит @a code = wire-значение panel status (0x00…0x24). */
using AwaitingStatus = uint64_t;

[[nodiscard]] constexpr AwaitingStatus statusMaskBit(msg::Status::Code code) noexcept {
    return 1ull << static_cast<uint8_t>(code);
}

constexpr AwaitingStatus kAwaitingNone = 0ull;
constexpr AwaitingStatus kAwaitingSuccessOnly = statusMaskBit(msg::Status::Code::Success);

/** Все именованные NIS Table 1 status в `msg::Status::Code` (без AppError / Unrecognized_Header). */
constexpr AwaitingStatus kAwaitingAllPanel =
    statusMaskBit(msg::Status::Code::Invalid_Instruction)
    | statusMaskBit(msg::Status::Code::Success)
    | statusMaskBit(msg::Status::Code::Invalid_CompId)
    | statusMaskBit(msg::Status::Code::Invalid_PageId)
    | statusMaskBit(msg::Status::Code::Invalid_PicId)
    | statusMaskBit(msg::Status::Code::Invalid_FontId)
    | statusMaskBit(msg::Status::Code::Invalid_FileOperation)
    | statusMaskBit(msg::Status::Code::Invalid_CRC)
    | statusMaskBit(msg::Status::Code::Invalid_BaudRate)
    | statusMaskBit(msg::Status::Code::Invalid_Waveform_ID_Channel)
    | statusMaskBit(msg::Status::Code::Invalid_VarName_Attr)
    | statusMaskBit(msg::Status::Code::Invalid_VarOperation)
    | statusMaskBit(msg::Status::Code::Failed_Assignment)
    | statusMaskBit(msg::Status::Code::Failed_Eeprom)
    | statusMaskBit(msg::Status::Code::Invalid_QuantityOfParameters)
    | statusMaskBit(msg::Status::Code::Failed_IO_Operation)
    | statusMaskBit(msg::Status::Code::Invalid_EscapeCharacter)
    | statusMaskBit(msg::Status::Code::VarName_TooLong)
    | statusMaskBit(msg::Status::Code::Serial_Overflow);

/** NIS §7 п.23 — Serial Buffer Overflow, не зависит от bkcmd. */
constexpr AwaitingStatus kAwaitingBkcmdIndependent = statusMaskBit(msg::Status::Code::Serial_Overflow);

/** NIS §7 Table 1 fail-байты (bkcmd 2|3). */
constexpr AwaitingStatus kAwaitingBkcmdOnFailure =
    kAwaitingAllPanel & ~(kAwaitingSuccessOnly | kAwaitingBkcmdIndependent);

[[nodiscard]] constexpr bool statusInMask(msg::Status::Code code, AwaitingStatus mask) noexcept {
    if (mask == kAwaitingNone)
        return false;
    return (mask & statusMaskBit(code)) != kAwaitingNone;
}

/** Status шлётся панелью при данном bkcmd (Table 1 + bkcmd-independent). */
[[nodiscard]] constexpr AwaitingStatus bkcmdAllowedStatus(BkCmd bkcmd) noexcept {
    switch (bkcmd) {
    case BkCmd::Off:
        return kAwaitingBkcmdIndependent;
    case BkCmd::OnSuccess:
        return kAwaitingSuccessOnly | kAwaitingBkcmdIndependent;
    case BkCmd::OnFailure:
        return kAwaitingBkcmdOnFailure | kAwaitingBkcmdIndependent;
    case BkCmd::Always:
        return kAwaitingAllPanel;
    }
    return kAwaitingAllPanel;
}

/** Не привязывать к route активной tx — глобальное событие UART (orphan 0,0). */
[[nodiscard]] constexpr bool isBkcmdIndependentStatus(msg::Status::Code code) noexcept {
    return code == msg::Status::Code::Serial_Overflow;
}

} // namespace nex
