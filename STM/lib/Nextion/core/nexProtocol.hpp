#pragma once
#include <cstdint>
#include <cstring>

// --- Кадр UART и разбор потока (до семантики сообщений Nextion) ---
namespace nex {

    namespace Physical {
        static constexpr uint8_t TERM_BYTE = 0xFF;
        static constexpr uint16_t TERM_COUNT = 3;
        /** Суффикс инструкции Nextion: 0xFF×3 (NIS §16). */
        inline constexpr uint8_t FRAME_TERMINATORS[TERM_COUNT] = { TERM_BYTE, TERM_BYTE, TERM_BYTE };
    }

    struct RxFrame {
        static constexpr uint16_t MAX_PAYLOAD = 64;
        uint8_t header = 0;
        uint8_t payload[MAX_PAYLOAD] = {};
        uint16_t length = 0;
    };

    /**
     * Исходящий кадр: `length` — сначала только полезная нагрузка (`serialize` в пределах `MAX_PAYLOAD`);
     * перед отправкой `TxFramer` дописывает `0xFF×3` в `payload[length..]`, тогда `length` включает терминаторы.
     */
    struct TxFrame {
        static constexpr uint16_t MAX_PAYLOAD = 61;
        uint8_t payload[MAX_PAYLOAD + Physical::TERM_COUNT]{};
        uint16_t length = 0;

        /**
         * Дописать `n` байт из `src` в конец полезной нагрузки (до `MAX_PAYLOAD`, терминаторы не считаются).
         * @return false — `n > 0` и `src == nullptr`, или не хватает места.
         */
        bool pushBytes(const void* src, uint16_t n) noexcept {
            if (n == 0u)
                return true;
            if (src == nullptr)
                return false;
            if (static_cast<uint32_t>(length) + static_cast<uint32_t>(n) > static_cast<uint32_t>(MAX_PAYLOAD))
                return false;
            std::memcpy(payload + length, src, n);
            length = static_cast<uint16_t>(length + n);
            return true;
        }
    };

} // namespace nex
