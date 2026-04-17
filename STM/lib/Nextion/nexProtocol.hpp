#pragma once
#include <cstdint>
#include <cstddef>
#include <stdint.h>
#include <type_traits>
#include <variant>

// --- PROTOCOL DEFINITIONS ---

namespace BIF {
class IByteStream;
}

namespace Nextion {
    namespace Physical {
        static constexpr uint8_t TERM_BYTE = 0xFF;
        static constexpr uint8_t TERM_COUNT = 3;
        /** Суффикс инструкции Nextion: 0xFF×3 (NIS §16). */
        inline constexpr uint8_t FRAME_TERMINATORS[TERM_COUNT] = { TERM_BYTE, TERM_BYTE, TERM_BYTE };
    }

    struct Config {
        static constexpr uint16_t RETRY_BUF = 64;
        static constexpr uint16_t TIMEOUT_MS = 150;
        /** Число попыток передачи одной команды (первая + повторы); `0` трактуется как `1` (без повторов после первой отправки). */
        static constexpr uint8_t SEND_RETRY_MAX = 3;
    };

    struct Frame {
        static constexpr uint16_t MAX_PAYLOAD = 64;
        uint8_t header;
        uint8_t payload[MAX_PAYLOAD];
        uint16_t length;
    };

    class FrameParser {
    public:
        enum class State : uint8_t { WaitHeader, Collect, WaitTerm };

        void reset();
        bool feed(uint8_t inputByte, Frame& outFrame);

    private:
        State _state = State::WaitHeader;
        Frame _current;
        uint8_t _terms = 0;
    };

    // NIS §7 — первый байт (обычно 0xFF×3). §1.16 transparent. См. Instruction Set.

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
                InvalidInstruction = 0x00,
                InstructionSuccess = 0x01,
                InvalidComponentId = 0x02,
                InvalidPageId = 0x03,
                InvalidPictureId = 0x04,
                InvalidFontId = 0x05,
                InvalidFileOperation = 0x06,
                InvalidCrc = 0x09,
                InvalidBaudRate = 0x11,
                InvalidWaveformIdOrChannel = 0x12,
                InvalidVariableNameOrAttribute = 0x1A,
                InvalidVariableOperation = 0x1B,
                AssignmentFailedToAssign = 0x1C,
                EepromOperationFailed = 0x1D,
                InvalidQuantityOfParameters = 0x1E,
                IoOperationFailed = 0x1F,
                EscapeCharacterInvalid = 0x20,
                VariableNameTooLong = 0x23,
            };
            Code status;
        };

        struct NumericResponse {
            constexpr static uint8_t Header = 0x71;
            int32_t value;
        };

        struct StringResponse {
            constexpr static uint8_t Header = 0x70;
            /** Копия полезной нагрузки кадра; не зависит от времени жизни `RawMessage`. */
            char chars[Frame::MAX_PAYLOAD]{};
            uint16_t length = 0;
            const char* data() const noexcept { return chars; }
        };

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

    void TranslateMessage(const Frame& f, Message& out);

    class NexGate {
        BIF::IByteStream& _stream;
        FrameParser _framer;
        char _retryBuf[Config::RETRY_BUF]{};
        uint32_t _lastTxMs = 0;
        uint8_t _txAttempt = 0;
        bool _isWaiting = false;

        /// true — все size байт записаны; false — обрыв (ошибка канала или TX-буфер полон в этом тике).
        static bool write_all(BIF::IByteStream& s, const uint8_t* data, size_t len);
        bool transmit(const char* c, uint32_t nowMs);

    public:
        explicit NexGate(BIF::IByteStream& s);
        /// false — команда не ушла целиком (см. write_all / getStatus); ожидание ответа не включается.
        bool send(const char* cmd, uint32_t nowMs);
        bool update(Message& out, uint32_t nowMs, bool* send_aborted = nullptr);
    };

} // namespace Nextion
