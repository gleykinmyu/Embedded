/**
 * @file idmx.hpp
 * @brief Контракт DMX512: Frame / Universe, раздельные iTx и iRx.
 *
 * Роли продуктовые: пульт = iTx, прибор = iRx. Тестер (RX↔TX на одном
 * USART) — исключение: композиция снаружи этих интерфейсов.
 *
 * Start code на провод добавляет транспорт (RS485 / sACN), не Frame.
 * RDM — отдельный интерфейс на том же драйвере.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace BIF {
namespace dmx {

inline constexpr std::size_t kMaxChannels = 512u;

enum class Status : uint8_t {
    OK = 0,
    OverFlowRX, ///< SW-буфер / UART overrun / UDP drop.
    DataError,  ///< Битый кадр (FE на RS485, невалидный ArtDmx/E1.31).
};

/// 512 каналов (уровни). Без start code и без count.
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

/// Адрес (Art-Net / sACN / логический слот) + данные каналов.
struct Universe {
    uint16_t id = 0;
    Frame data{};
};

/**
 * Источник DMX (пульт / контроллер).
 * `bind` запоминает указатель (буфер живёт снаружи); `send()` без аргументов.
 * Наследник хранит ссылку: один порт — один указатель, Art-Net — таблица вселенных.
 */
class iTx {
public:
    virtual ~iTx() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;

    /// Привязать буфер. `uni->id` — номер потока. `nullptr` — снять все.
    virtual bool bind(const Universe* uni) = 0;
    /// Снять одну вселенную (тот же указатель, что в bind).
    virtual bool unbind(const Universe* uni) = 0;
    /// Отправить все привязанные. Не блокирует.
    virtual bool send() = 0;

    [[nodiscard]] virtual Status getStatus() = 0;
    virtual void clearErrors() = 0;

    [[nodiscard]] virtual uint32_t frameCount() const = 0;
};

/**
 * Приёмник DMX (прибор / monitor).
 * `bind` запоминает, куда писать кадр; `recv()` без аргументов.
 * `poll` копит байты у транспорта без своего прерывания (UDP). RS485 его не вызывает.
 */
class iRx {
public:
    virtual ~iRx() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;

    /// Привязать буфер. `uni->id` — какой поток принимать. `nullptr` — снять все.
    virtual bool bind(Universe* uni) = 0;
    virtual bool unbind(Universe* uni) = 0;
    virtual void poll() {}
    /// Забрать кадр в привязанный Universe. true — буфер обновлён.
    virtual bool recv() = 0;

    /// 0 или 1: есть непрочитанный кадр.
    [[nodiscard]] virtual std::size_t available() const = 0;
    virtual void purge() = 0;

    [[nodiscard]] virtual Status getStatus() = 0;
    virtual void clearErrors() = 0;

    [[nodiscard]] virtual uint32_t frameCount() const = 0;
};

} // namespace dmx
} // namespace BIF
