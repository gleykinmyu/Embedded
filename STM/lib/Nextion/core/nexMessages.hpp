#pragma once
#include <cstdint>
#include <variant>
#include "nexProtocol.hpp"

namespace nex {
    namespace msg {

        struct Unknown {
            enum class Reason : uint8_t {
                /** Сообщение ещё не заполнялось (`Message{}`, до `MessageMaker`). */
                UninitializedMessage = 0,
                UnrecognizedHeader,
            };
            Reason reason  = Reason::UninitializedMessage;
            uint8_t header = 0;
        };

        struct StatusResponse {
            enum class Code : uint8_t {
                InvalidInstruction   = 0x00,
                InstructionSuccess   = 0x01,
                InvalidComponentId   = 0x02,
                InvalidPageId        = 0x03,
                InvalidPictureId     = 0x04,
                InvalidFontId        = 0x05,
                InvalidFileOperation = 0x06,
                InvalidCrc           = 0x09,
                InvalidBaudRate      = 0x11,
                InvalidWaveformIdOrChannel     = 0x12,
                InvalidVariableNameOrAttribute = 0x1A,
                InvalidVariableOperation = 0x1B,
                AssignmentFailedToAssign = 0x1C,
                EepromOperationFailed    = 0x1D,
                InvalidQuantityOfParameters = 0x1E,
                IoOperationFailed      = 0x1F,
                EscapeCharacterInvalid = 0x20,
                VariableNameTooLong    = 0x23,
            };
            Code status;
        };

        struct NumericResponse {
            constexpr static uint8_t Header = 0x71;
            int32_t value;
        };

        struct StringResponse {
            constexpr static uint8_t Header = 0x70;
            /** Копия полезной нагрузки кадра; не зависит от времени жизни `InputFrame`. */
            char chars[RxFrame::MAX_PAYLOAD]{};
            uint16_t length = 0;
            const char* data() const noexcept { return chars; }
        };

        /** Код нажатия/отпускания в кадре 0x65 и второй аргумент UART-инструкции `click` (NIS, 0 / 1). */
        enum class TouchState : uint8_t {
            Release = 0x00,
            Press = 0x01
        };

        struct TouchCompEvent {
            constexpr static uint8_t Header = 0x65;
            uint8_t page_id;
            uint8_t component_id;
            TouchState state;
        };

        enum class TouchPlane : uint8_t {
            Awake = 0x67,
            Sleep = 0x68,
        };

        struct TouchXYEvent {
            constexpr static uint8_t Header = 0x67;
            TouchPlane plane;
            uint16_t x;
            uint16_t y;
            TouchState state;
        };

        struct PageEvent {
            constexpr static uint8_t Header = 0x66;
            uint8_t page_id;
        };

        struct SystemEvent {
            enum class Code : uint8_t {
                SerialBufferOverflow = 0x24,
                AutoEnteredSleepMode = 0x86,
                AutoWakeFromSleep = 0x87,
                NextionReady = 0x88,
                StartMicroSdUpgrade = 0x89,
                /** Transparent Data Mode: блок принят (NIS §1.16). */
                TransparentBlockComplete = 0xFD,
                /** Transparent Data Mode: готов принять блок. */
                TransparentReadyToReceive = 0xFE,
                StartupPreamble = 0xFF,
            };
            Code code;
        };

    } // namespace msg

    using Message = std::variant<msg::Unknown,
        msg::StatusResponse,
        msg::NumericResponse,
        msg::StringResponse,
        msg::TouchCompEvent,
        msg::TouchXYEvent,
        msg::PageEvent,
        msg::SystemEvent>;

} // namespace nex
