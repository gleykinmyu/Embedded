/**
 * @file dmx_data.hpp
 * @brief Кадр и вселенная DMX. Без транспорта и без замка.
 *
 * Кто имеет право писать каналы, задаёт приложение (один вход, много выходов).
 * Занятость передачи — у порта: пока dmx::wire::Transceiver::isBusy(), буфер его.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace dmx {

inline constexpr std::size_t kMaxChannels = 512u;

enum class Status : uint8_t {
    OK = 0,
    OverFlowRX, ///< UART overrun, пока копились слоты.
    DataError,  ///< Битый кадр на линии.
};

/// 512 уровней. Start code в кадр не входит — его ставит транспорт.
struct Frame {
    uint8_t channels[kMaxChannels]{};

    void clear() noexcept
    {
        for (std::size_t i = 0; i < kMaxChannels; ++i)
            channels[i] = 0;
    }

    void fill(uint8_t v) noexcept
    {
        for (std::size_t i = 0; i < kMaxChannels; ++i)
            channels[i] = v;
    }

    /// Канал 1…512; иначе 0.
    [[nodiscard]] uint8_t get(uint16_t ch) const noexcept
    {
        if (ch == 0u || ch > kMaxChannels)
            return 0;
        return channels[ch - 1u];
    }

    void set(uint16_t ch, uint8_t v) noexcept
    {
        if (ch == 0u || ch > kMaxChannels)
            return;
        channels[ch - 1u] = v;
    }
};

/// Логический поток: номер (Art-Net / sACN) и уровни.
struct Universe {
    uint16_t id = 0;
    Frame data{};
};

} // namespace dmx
