/**
 * @file idmx.hpp
 * @brief Интерфейсы потока DMX512 (кадр: Break/MAB + start code + каналы).
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace dmx {

inline constexpr uint32_t kBaud = 250000u;
inline constexpr std::size_t kMaxChannels = 512u;
inline constexpr std::size_t kFrameSize = 1u + kMaxChannels; ///< start code + 512 каналов
inline constexpr uint16_t kBreakUs = 100u; ///< ≥88 µs по ANSI E1.11
inline constexpr uint16_t kMabUs = 12u;    ///< ≥8 µs Mark After Break

/// Общий lifecycle порта DMX.
class IDmxStream {
public:
    virtual ~IDmxStream() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
};

/// Передатчик: Break + MAB + слоты.
class IDmxTransmitter : public IDmxStream {
public:
    /**
     * Отправить кадр: Break + MAB + startCode + data[0..n).
     * @param n число каналов (без start code), clamp до kMaxChannels.
     */
    virtual bool send(uint8_t startCode, const uint8_t* data, std::size_t n) = 0;
};

/// Приёмник: sync по Break, сборка кадра.
class IDmxReceiver : public IDmxStream {
public:
    /**
     * Забрать последний полный (или завершённый Break-ом) кадр.
     * @param maxN ёмкость data[]; @param n фактически скопированных каналов.
     */
    virtual bool receive(uint8_t& startCode, uint8_t* data, std::size_t maxN, std::size_t& n) = 0;

    /// Вычитать UART в state machine.
    virtual void poll() = 0;
};

} // namespace dmx
