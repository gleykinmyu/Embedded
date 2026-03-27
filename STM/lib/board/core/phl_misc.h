#include "config.h"

#pragma once
#include "stm32f4xx_hal.h"

    namespace UART {
        enum class Baud : uint32_t {
            B9600 = 9600, B19200 = 19200, B38400 = 38400, 
            B57600 = 57600, B115200 = 115200, B921600 = 921600
        };

        class Frame {
        public:
            // Переименовано в Frame_t
            enum Frame_t : uint8_t {
                f8N1, f8E1, f8O1, f8N2, f8E2, f8O2,
                f9N1, f9E1, f9O1, f9N2, f9E2, f9O2
            };
        private:
            Frame_t _frame;
        public:
            constexpr Frame(Frame_t f = f8N1) : _frame(f) {}

            // Оператор приведения для лаконичного использования в switch/if
            constexpr operator Frame_t() const { return _frame; }

            static constexpr Frame Default() { return Frame(f8N1); }
            static constexpr Frame Modbus()  { return Frame(f8E1); }
        };
    } // namespace UART

namespace PHL {

    /**
     * @brief Универсальный статус операции PHL.
     * Оборачивает HAL_StatusTypeDef, предоставляя удобный интерфейс.
     */
    class Result {
    public:
        enum Value : uint8_t {
            OK      = HAL_OK,      // 0x00
            Error   = HAL_ERROR,   // 0x01
            Busy    = HAL_BUSY,    // 0x02
            Timeout = HAL_TIMEOUT  // 0x03
        };

    private:
        Value _value;

    public:
        // Конструктор автоматической конвертации из HAL
        constexpr Result(HAL_StatusTypeDef status) 
            : _value(static_cast<Value>(status)) {}

        // Конструктор из внутреннего Enum
        constexpr Result(Value v) : _value(v) {}

        // Операторы сравнения для синтаксиса: if (res == Result::OK)
        constexpr bool operator==(Value v) const { return _value == v; }
        constexpr bool operator!=(Value v) const { return _value != v; }
        
        // Позволяет использовать объект напрямую в switch
        constexpr operator Value() const { return _value; }

        // Метод быстрой проверки
        constexpr bool IsOK() const { return _value == OK; }

        /**
         * @brief Текстовое описание статуса (для логов).
         */
        const char* GetStr() const {
            switch (_value) {
                case OK:      return "OK";
                case Error:   return "ERROR";
                case Busy:    return "BUSY";
                case Timeout: return "TIMEOUT";
                default:      return "UNKNOWN";
            }
        }
    };

} // namespace PHL
