#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// --- Кадр UART и разбор потока (до семантики сообщений Nextion) ---
namespace nex {

    /** Физический уровень кадра: байт-терминатор и массив `0xFF×3` (NIS §16). */
    namespace Physical {
        static constexpr uint8_t TERM_BYTE = 0xFF;
        static constexpr uint16_t TERM_COUNT = 3;
        /** Суффикс инструкции Nextion: 0xFF×3 (NIS §16). */
        inline constexpr uint8_t FRAME_TERMINATORS[TERM_COUNT] = { TERM_BYTE, TERM_BYTE, TERM_BYTE };
    }

    /**
     * **R**eceive **frame** — один принятый кадр от дисплея: заголовок + полезная нагрузка (до `MAX_PAYLOAD`).
     */
    struct RxFrame {
        static constexpr uint16_t MAX_PAYLOAD = 64;
        uint8_t header = 0;
        uint8_t payload[MAX_PAYLOAD] = {};
        uint16_t length = 0;
    };

    /**
     * **T**ransmit **frame** — буфер исходящего кадра к дисплею.
     * `length` — только полезная нагрузка (`serialize` ≤ `MAX_PAYLOAD`); `TxFramer` дописывает `0xFF×3`.
     */
    struct TxFrame {
        static constexpr uint16_t MAX_PAYLOAD = 160; //TODO сделать настраиваемым
        uint8_t payload[MAX_PAYLOAD + Physical::TERM_COUNT]{};
        uint16_t length = 0;
    };

    /** Запас под префикс `xstr` и числовые поля до quoted-текста (`gui::TextInRegion`). */
    constexpr std::size_t kXstrCommandOverhead = 80u;

    /** Макс. длина строки в `xstr` (байт текста в кавычках, ≤ `TxFrame::MAX_PAYLOAD`). */
    [[nodiscard]] constexpr std::size_t maxXstrTextLength() noexcept {
        return (static_cast<std::size_t>(TxFrame::MAX_PAYLOAD) > kXstrCommandOverhead)
            ? static_cast<std::size_t>(TxFrame::MAX_PAYLOAD) - kXstrCommandOverhead
            : 0u;
    }

} // namespace nex
