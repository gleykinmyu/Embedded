#pragma once

#include <cstdint>

namespace nex {

/**
 * Цвет RGB565 для Nextion (`raw`). Каналы 565 в API — `r`, `g`, `b`; 8-bit — параметры `r8`, `g8`, `b8`.
 * Именованные значения — `Color::std` (NIS: одно 16-битное слово, часто десятичное 0…65535; R[15:11], G[10:5], B[4:0]).
 */
struct Color {
    /** Стандартные цвета RGB565. */
    enum class std : uint16_t {
        Black   = 0x0000,
        White   = 0xFFFF,
        Red     = 0xF800,
        Green   = 0x07E0,
        Blue    = 0x001F,
        Yellow  = 0xFFE0,
        Cyan    = 0x07FF,
        Magenta = 0xF81F,
        Orange  = 0xFC00,
        Purple  = 0x781F,
        Gray    = 0x8410,
        Navy    = 0x0010,
        Maroon  = 0x8000,
        Silver  = 0xC618,
        Olive   = 0x8400,
        Teal    = 0x0410,
    };

    uint16_t raw{};

    constexpr Color() noexcept = default;
    /** Готовое слово RGB565 (как в NIS / из регистра дисплея). */
    constexpr explicit Color(uint16_t packed565) noexcept : raw(packed565) {}

    constexpr void set(std c) noexcept { raw = static_cast<uint16_t>(c); }

    /** Полная перезапись `raw` из каналов 565: r ∈ [0,31], g ∈ [0,63], b ∈ [0,31]. */
    constexpr void set_rgb(uint8_t r, uint8_t g, uint8_t b) noexcept {
        raw = static_cast<uint16_t>(
            (static_cast<uint16_t>(r & 0x1Fu) << 11) | (static_cast<uint16_t>(g & 0x3Fu) << 5) |
            static_cast<uint16_t>(b & 0x1Fu));
    }

    /** Полная перезапись `raw` из 8-bit RGB с округлением к шагу 565. */
    constexpr void set_rgb8(uint8_t r8, uint8_t g8, uint8_t b8) noexcept {
        const uint16_t r = static_cast<uint16_t>((static_cast<uint16_t>(r8) * 31u + 127u) / 255u) & 0x1Fu;
        const uint16_t g = static_cast<uint16_t>((static_cast<uint16_t>(g8) * 63u + 127u) / 255u) & 0x3Fu;
        const uint16_t b = static_cast<uint16_t>((static_cast<uint16_t>(b8) * 31u + 127u) / 255u) & 0x1Fu;
        raw = static_cast<uint16_t>((r << 11) | (g << 5) | b);
    }

    constexpr void set_r(uint8_t r) noexcept {
        raw = static_cast<uint16_t>((raw & 0x07FFu) | (static_cast<uint16_t>(r & 0x1Fu) << 11));
    }
    constexpr void set_g(uint8_t g) noexcept {
        raw = static_cast<uint16_t>((raw & 0xF81Fu) | (static_cast<uint16_t>(g & 0x3Fu) << 5));
    }
    constexpr void set_b(uint8_t b) noexcept {
        raw = static_cast<uint16_t>((raw & 0xFFE0u) | static_cast<uint16_t>(b & 0x1Fu));
    }

    constexpr void set_r8(uint8_t r8) noexcept {
        const uint16_t r = static_cast<uint16_t>((static_cast<uint16_t>(r8) * 31u + 127u) / 255u) & 0x1Fu;
        raw = static_cast<uint16_t>((raw & 0x07FFu) | (r << 11));
    }
    constexpr void set_g8(uint8_t g8) noexcept {
        const uint16_t g = static_cast<uint16_t>((static_cast<uint16_t>(g8) * 63u + 127u) / 255u) & 0x3Fu;
        raw = static_cast<uint16_t>((raw & 0xF81Fu) | (g << 5));
    }
    constexpr void set_b8(uint8_t b8) noexcept {
        const uint16_t b = static_cast<uint16_t>((static_cast<uint16_t>(b8) * 31u + 127u) / 255u) & 0x1Fu;
        raw = static_cast<uint16_t>((raw & 0xFFE0u) | b);
    }

    constexpr uint8_t r() const noexcept { return static_cast<uint8_t>((raw >> 11) & 0x1Fu); }
    constexpr uint8_t g() const noexcept { return static_cast<uint8_t>((raw >> 5) & 0x3Fu); }
    constexpr uint8_t b() const noexcept { return static_cast<uint8_t>(raw & 0x1Fu); }

    constexpr explicit operator uint16_t() const noexcept { return raw; }

    constexpr Color& operator=(std c) noexcept {
        set(c);
        return *this;
    }
};

constexpr bool operator==(Color a, Color b) noexcept { return a.raw == b.raw; }
constexpr bool operator!=(Color a, Color b) noexcept { return a.raw != b.raw; }

constexpr bool operator==(Color a, Color::std b) noexcept { return a.raw == static_cast<uint16_t>(b); }
constexpr bool operator==(Color::std a, Color b) noexcept { return static_cast<uint16_t>(a) == b.raw; }
constexpr bool operator!=(Color a, Color::std b) noexcept { return !(a == b); }
constexpr bool operator!=(Color::std a, Color b) noexcept { return !(a == b); }

constexpr bool operator==(Color::std a, Color::std b) noexcept {
    return static_cast<uint16_t>(a) == static_cast<uint16_t>(b);
}
constexpr bool operator!=(Color::std a, Color::std b) noexcept { return !(a == b); }

} // namespace nex
