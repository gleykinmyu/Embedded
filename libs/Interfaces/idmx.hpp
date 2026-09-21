/**
 * @file idmx.hpp
 * @brief Публичный контракт потока DMX512: один universe, приём и передача.
 *
 * Транспорт (RS485 / Art-Net / sACN) скрыт за IDmx — как ICAN скрывает bxCAN.
 * Кадр: start code + до 512 слотов. Каналы в UI 1…512 ↔ slots[0…511].
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

enum class Direction : uint8_t {
    Receive = 0,
    Transmit = 1,
};

enum class Transport : uint8_t {
    Rs485 = 0,
    ArtNet = 1,
    Sacn = 2,
};

enum class Status : uint8_t {
    OK = 0,
    OverFlowRX, ///< SW-буфер / UART overrun / UDP drop.
    DataError,  ///< Битый кадр (FE на RS485, невалидный ArtDmx/E1.31).
};

inline const char* cstr(Status s) noexcept
{
    switch (s) {
    case Status::OK: return "OK";
    case Status::OverFlowRX: return "OverFlowRX";
    case Status::DataError: return "DataError";
    default: return "?";
    }
}

inline const char* cstr(Transport t) noexcept
{
    switch (t) {
    case Transport::Rs485: return "RS485";
    case Transport::ArtNet: return "Art-Net";
    case Transport::Sacn: return "sACN";
    default: return "?";
    }
}

inline const char* cstr(Direction d) noexcept
{
    return d == Direction::Transmit ? "TX" : "RX";
}

/// Один DMX-кадр (start code отдельно от слотов).
struct Frame {
    uint8_t startCode = 0;
    uint16_t count = 0; ///< Число слотов в кадре (0…512).
    uint8_t slots[kMaxChannels]{};

    void clear() noexcept
    {
        startCode = 0;
        count = 0;
        for (std::size_t i = 0; i < kMaxChannels; ++i)
            slots[i] = 0;
    }

    void fill(uint8_t v, uint16_t n = static_cast<uint16_t>(kMaxChannels)) noexcept
    {
        if (n > kMaxChannels)
            n = static_cast<uint16_t>(kMaxChannels);
        count = n;
        for (uint16_t i = 0; i < n; ++i)
            slots[i] = v;
    }

    /// Канал 1…512.
    [[nodiscard]] uint8_t get(uint16_t ch) const noexcept
    {
        if (ch == 0u || ch > count)
            return 0;
        return slots[ch - 1u];
    }

    void set(uint16_t ch, uint8_t v) noexcept
    {
        if (ch == 0u || ch > kMaxChannels)
            return;
        slots[ch - 1u] = v;
        if (ch > count)
            count = ch;
    }
};

/**
 * Порт DMX: lifecycle, направление, один входящий кадр, отправка кадра.
 *
 * - `send` / `recv` не блокируют; `recv` забирает последний полный кадр.
 * - `poll()` вычитывать UART / UDP (вызывать из главного цикла).
 * - RS485 half-duplex: `setDirection` переключает DE/RE; Rx+Tx одновременно нельзя.
 * - Art-Net / sACN: направление — роль тестера; физически UDP полный дуплекс.
 * - `universe`: Art-Net 15 bit (net.sub.uni), sACN 1…63999, RS485 — логический номер для UI.
 */
class IDmx {
public:
    virtual ~IDmx() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;

    [[nodiscard]] virtual Transport transport() const = 0;
    [[nodiscard]] virtual Direction direction() const = 0;
    virtual bool setDirection(Direction dir) = 0;

    [[nodiscard]] virtual uint16_t universe() const = 0;
    virtual void setUniverse(uint16_t universe) = 0;

    virtual bool send(const Frame& frame) = 0;
    virtual bool recv(Frame& frame) = 0;

    virtual void poll() = 0;

    /// 0 или 1: есть непрочитанный RX-кадр.
    [[nodiscard]] virtual std::size_t available() const = 0;
    virtual void purge() = 0;

    virtual Status getStatus() = 0;
    virtual void clearErrors() = 0;

    /// Счётчик успешно принятых (RX) или отправленных (TX) кадров.
    [[nodiscard]] virtual uint32_t frameCount() const = 0;
};

} // namespace dmx
