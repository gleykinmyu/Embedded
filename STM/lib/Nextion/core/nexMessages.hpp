#pragma once
#include <cstdint>
#include <variant>
#include "nexTypes.hpp"
#include "nexProtocol.hpp"

namespace nex {
    namespace msg {

        /**
         * События уровня приложения / MCU: неинициализированное или неразобранное сообщение с шины,
         * либо результат завершения исходящей команды (маршрутизация по `page_id` / `component_id`).
         */
        struct AppEvent {
            enum class Kind : uint8_t {
                /** `Message{}` до разбора кадра. */
                UninitializedMessage = 0,
                /** Заголовок кадра не распознан (`TranslateMessage`). */
                UnrecognizedHeader,
                /** Результат завершения команды на стороне MCU. */
                CommandResult,
            };

            enum class Outcome : uint8_t {
                Completed = 0,
                DeviceStatus,
                Timeout,
                TxError,
                QueueRejected,
            };

            Kind kind = Kind::UninitializedMessage;

            /** Для `UnrecognizedHeader` — байт заголовка кадра. */
            uint8_t header = 0;

            /** Для `CommandResult` — метаданные команды и исход. */
            uint8_t page_id = 0u;
            uint8_t component_id = 0u;
            uint16_t token = 0u;
            Outcome outcome = Outcome::Completed;
            /** Для `outcome == DeviceStatus` — `msg::StatusResponse::Code`, иначе 0. */
            uint8_t code = 0u;

            static AppEvent unrecognizedHeader(uint8_t h) noexcept {
                AppEvent e{};
                e.kind = Kind::UnrecognizedHeader;
                e.header = h;
                return e;
            }

            static AppEvent commandResult(uint8_t page_id, uint8_t component_id, uint16_t token, Outcome outcome,
                uint8_t code = 0u) noexcept {
                AppEvent e{};
                e.kind = Kind::CommandResult;
                e.page_id = page_id;
                e.component_id = component_id;
                e.token = token;
                e.outcome = outcome;
                e.code = code;
                return e;
            }
        };

        /**
         * Ответ **status** — результат выполнения инструкции (байт кода 0x00… из протокола Nextion).
         */
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

        /** Ответ с числом (заголовок **0x71**, например после `get` числового атрибута). */
        struct NumericResponse {
            constexpr static uint8_t Header = 0x71;
            int32_t value;
        };

        /** Ответ со строкой (заголовок **0x70** — текстовое значение атрибута). */
        struct StringResponse {
            constexpr static uint8_t Header = 0x70;
            /** Копия полезной нагрузки кадра; не зависит от времени жизни `InputFrame`. */
            char chars[RxFrame::MAX_PAYLOAD]{};
            uint16_t length = 0;
            const char* data() const noexcept { return chars; }
        };

        /**
         * Событие касания по **comp**onent — страница, id компонента, press/release (кадр **0x65**).
         */
        struct TouchEvent {
            constexpr static uint8_t Header = 0x65;
            uint8_t page_id;
            uint8_t component_id;
            TouchState state;
        };

        enum class TouchPlane : uint8_t {
            Awake = 0x67,
            Sleep = 0x68,
        };

        /**
         * Событие касания по координатам X и Y (пробуждение или сон; заголовки 0x67 и 0x68).
         */
        struct TouchXYEvent {
            constexpr static uint8_t Header = 0x67;
            TouchPlane plane;
            Point pos{};
            TouchState state;
        };

        /** Смена страницы на дисплее (заголовок **0x66**, индекс страницы). */
        struct PageEvent {
            constexpr static uint8_t Header = 0x66;
            uint8_t page_id;
        };

        /**
         * Системное событие дисплея (переполнение буфера, сон, готовность, SD-обновление, **T**ransparent **D**ata и т.д.).
         */
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

    /** Разобранное входящее сообщение дисплея — один из типов в `msg::`. */
    using Message = std::variant<msg::AppEvent,
        msg::StatusResponse,
        msg::NumericResponse,
        msg::StringResponse,
        msg::TouchEvent,
        msg::TouchXYEvent,
        msg::PageEvent,
        msg::SystemEvent>;

} // namespace nex
