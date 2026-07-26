#pragma once

#include "ibyte_stream.hpp"

namespace BIF {

/**
 * Поток байтов с полным дуплексным `exchange` (типично SPI master).
 *
 * `tx == nullptr` → на шину 0xFF; `rx == nullptr` → ответ отбрасывается.
 * Реализация может быть polling (без RX-кольца) — предпочтительно для flash/транзакций под CS.
 */
class IExByteStream : public IByteStream {
public:
    ~IExByteStream() override = default;

    [[nodiscard]] virtual bool exchange(const uint8_t* tx, uint8_t* rx, size_t n) noexcept = 0;
};

} // namespace BIF
