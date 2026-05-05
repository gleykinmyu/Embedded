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


/**
 * Атрибут `sta` (фон / заливка) в NIS для ветки BG: crop | color | image | transparent.
 * Коды 0…3 совместимы с xstr / типичным Attribute Pane для текстовых виджетов.
 */
enum class BGStyle : uint8_t {
    CropImage    = 0,
    Color        = 1,
    Image        = 2,
    Transparent  = 3,
};

/** Идентификатор ресурса картинки/страницы в NIS (`pic`, `pic2`, `cpic`, …). */
using PicId = uint16_t;

/** Идентификатор шрифта в NIS (`font`). */
using FontId = uint16_t;

/** Пиксельная координата по оси X или Y на экране Nextion (NIS). */
using Coord = int16_t;

/** Точка (x, y) на экране в пикселях. */
struct Point {
    int16_t x{};
    int16_t y{};

    constexpr Point() noexcept = default;
    constexpr Point(int16_t px, int16_t py) noexcept : x(px), y(py) {}
};

    /**
     * Представление строкового литерала (или массива `const char[N]`): длина без завершающего NUL, указатель на первый символ.
     * Сконструировать из `const char*` нельзя — только из `const char (&)[N]` (литерал `"..."` и т.п.).
     * Копирование копирует только `data`/`len` (тот же буфер литерала). Перемещение и присваивание запрещены.
     */
    class Literal {
    public:
        const char* const data;
        const uint8_t len;

        template<std::size_t N>
        constexpr Literal(const char (&s)[N]) noexcept
            : data(s)
            , len(static_cast<uint8_t>(N > 0u ? N - 1u : 0u))
        {
            static_assert(N <= 256u, "nex::Literal: строка длиннее 255 символов (экран не поддерживает)");
        }

        constexpr Literal(const Literal&) noexcept = default;
        Literal(Literal&&) = delete;
        Literal& operator=(const Literal&) = delete;
        Literal& operator=(Literal&&) = delete;
    };


} // namespace nex
