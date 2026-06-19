#pragma once
#include <cstdint>
#include <variant>
#include "nexTypes.hpp"
#include "nexProtocol.hpp"

namespace nex {
    namespace msg {

        /**
         * Ответ **status** — результат выполнения инструкции (байт кода 0x00…0x24 из протокола Nextion).
         */
        struct Status {
            /** Битовая маска кодов panel status (0x00…0x24); бит @a n = `1ull << code`. */
            using Mask = uint64_t;

            enum class Code : uint8_t {
                Invalid_Instruction = 0x00,
                Success        = 0x01,
                Invalid_CompId = 0x02,
                Invalid_PageId = 0x03,
                Invalid_PicId  = 0x04,
                Invalid_FontId = 0x05,
                Invalid_FileOperation = 0x06,
                Invalid_CRC    = 0x09,
                Invalid_BaudRate = 0x11,
                Invalid_Waveform_ID_Channel = 0x12,
                Invalid_VarName_Attr = 0x1A,
                Invalid_VarOperation = 0x1B,
                Failed_Assignment = 0x1C,
                Failed_Eeprom     = 0x1D,
                Invalid_QuantityOfParameters = 0x1E,
                Failed_IO_Operation     = 0x1F,
                Invalid_EscapeCharacter = 0x20,
                VarName_TooLong     = 0x23,
                Serial_Overflow     = 0x24,
                /** Синтетический отчёт MCU о завершении очередной команды (`tag_1`/`tag_2` — служебные поля). */
                AppError = 0xFB,
                Unrecognized_Header = 0xFF //Не существует в NIS
            };
            Code status = Code::Invalid_Instruction;

            /** С шины — обычно не заполняются; зарезервированы для расширений MCU. */
            uint8_t tag_1 = 0u;
            uint16_t tag_2 = 0u;

            [[nodiscard]] constexpr bool isOK() const noexcept { return status == Code::Success; }

            [[nodiscard]] static constexpr Mask maskBit(Code code) noexcept {
                return 1ull << static_cast<uint8_t>(code);
            }

            /** Есть ли код `status` в битовой маске. */
            [[nodiscard]] constexpr bool codeInMask(Mask mask) const noexcept {
                if (mask == 0ull)
                    return false;
                return (mask & maskBit(status)) != 0ull;
            }
        };

        [[nodiscard]] constexpr bool operator==(const Status& a, const Status& b) noexcept
        {
            return a.status == b.status && a.tag_1 == b.tag_1 && a.tag_2 == b.tag_2;
        }

        [[nodiscard]] constexpr bool operator!=(const Status& a, const Status& b) noexcept
        {
            return !(a == b);
        }

        /** Ответ с числом (заголовок **0x71**, например после `get` числового атрибута). */
        struct getNumeric {
            constexpr static uint8_t Header = 0x71;
            int32_t value;
        };

        /** Ответ со строкой (заголовок **0x70** — текстовое значение атрибута). */
        struct getString {
            constexpr static uint8_t Header = 0x70;
            char chars[RxFrame::MAX_PAYLOAD]{};
            uint16_t length = 0;
        };

        /**
         * Событие касания по **comp**onent — страница, id компонента, press/release (кадр **0x65**).
         */
        struct evTouch {
            constexpr static uint8_t Header = 0x65;
            uint8_t page_id;
            uint8_t comp_id;
            TouchState state;
        };

        /**
         * Координаты касания по UART (NIS §7.21–7.22, `sendxy`): заголовок **0x67** (awake) или **0x68** (sleep).
         */
        struct evTouchXY {
            enum class Mode : uint8_t {
                Awake = 0x67,
                Sleep = 0x68,
            };
            Mode mode;
            Point pos{};
            TouchState state;
        };

        /** Смена страницы на дисплее (заголовок **0x66**, индекс страницы). */
        struct evPage {
            constexpr static uint8_t Header = 0x66;
            uint8_t page_id;
        };

        /** Системное событие дисплея (сон, готовность, SD-обновление и т.д.). */
        struct evSystem {
            enum class Code : uint8_t {
                AutoEnteredSleepMode = 0x86,
                AutoWakeFromSleep = 0x87,
                NextionReady = 0x88,
                StartMicroSdUpgrade = 0x89,
                StartupPreamble = 0xFA //Не существует в NIS
            };
            Code code;
        };

        /**
         * Transparent Data Mode (NIS §1.16) и служебные события сессии приложения.
         * Коды `0xFD` / `0xFE` — заголовки кадров с дисплея; диапазон `0x01`…`0x7F` — внутренние (MCU).
         */
        struct evTransparent {
            enum class Code : uint8_t {
                // --- внутренние (MCU, не из `TranslateMessage`) ---
                // (добавлять по мере необходимости, например очередь / таймаут / отмена)

                /** NIS §1.16 — дисплей принял блок сырых данных. */
                BlockComplete = 0xFD,
                /** NIS §1.16 — дисплей готов принять блок сырых данных. */
                ReadyToReceive = 0xFE,
            };

            Code code;
        };

        /** Закрытие MCU `MsgBox` по кнопке (внутреннее, не с UART). */
        struct evMsgBox {
            enum class Action : uint8_t {
                None = 0,
                Ok = 0x02,
                Yes = 0x03,
                No = 0x04,
                Cancel = 0x05,
            };

            /** Зарезервирован для `MsgBox::showError` (прочие `tag` — код вызывающего). */
            static constexpr uint8_t kTagError = 0xFFu;

            uint8_t page_id = 0u;
            uint8_t comp_id = 0u;
            uint8_t tag = 0u;
            Action action = Action::None;
        };

    } // namespace msg

    /** Разобранное входящее сообщение дисплея — один из типов в `msg::`. */
    using Message = std::variant<msg::Status,
        msg::getNumeric,
        msg::getString,
        msg::evTouch,
        msg::evTouchXY,
        msg::evPage,
        msg::evSystem,
        msg::evTransparent,
        msg::evMsgBox>;

inline const char* cstr(msg::Status::Code c) noexcept {
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
