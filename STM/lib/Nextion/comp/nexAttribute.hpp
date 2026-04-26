#pragma once

#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "nexComponentBase.hpp"
#include "../nexTransceiver.hpp"

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

/**
 * Связка «имя атрибута NIS» + «тип значения в MCU» + «компонент» (чтение/запись зеркала на MCU).
 * Только атрибуты с записью; только чтение — отдельно (не этот класс).
 *
 * @tparam Name указатель на строку имени атрибута: статические массивы из `nex::prop::AttributeNames`
 *         (и вложенных структур), годные как NTTP `const char*` в C++17.
 * @tparam T логический тип значения атрибута (`Color`, `int32_t`, буфер текста и т.д.).
 *
 * Второй аргумент конструктора — ссылка на поле-зеркало на MCU (тот же смысл, что атрибут в NIS).
 * Чтение только через `value()` / `const` `value()` (ссылка на зеркало; копию при необходимости делайте явно).
 * Запись: `attr = v` (`v` — тип `T` или совместимый).
 *
 * `pushAssign(tx)` — сформировать кадр `objPath.Name=<десятичное int32>` (`cmd::assign::Numeric`) и вызвать `Transceiver::pushCommand`.
 * Префикс объекта: при `!component().isGlobal()` — `objname`; при `isGlobal()` — `p<Page::ID>.objname` (доступ с других страниц в NIS).
 */
template<const char* Name, typename T>
class Attribute {
    Component& _comp;
    T& _ref;

    /** Буфер под префикс объекта в UART-команде (с запасом под `p255.` + имя). */
    static constexpr size_t kObjPathCap = 40;

    bool fillObjectPath(char (&buf)[kObjPathCap]) const noexcept {
        const char* n = _comp.name;
        if (n == nullptr || n[0] == '\0')
            return false;
        if (!_comp.isGlobal()) {
            size_t i = 0;
            for (; n[i] != '\0' && i + 1 < kObjPathCap; ++i)
                buf[i] = n[i];
            buf[i] = '\0';
            return n[i] == '\0';
        }
        const int w = std::snprintf(buf, kObjPathCap, "p%u.%s", static_cast<unsigned>(_comp.page.ID), n);
        return w > 0 && static_cast<size_t>(w) < kObjPathCap;
    }

public:
    using value_type = T;
    using component_type = Component;

    constexpr Attribute(Component& c, T& value) noexcept : _comp(c), _ref(value) {}

    Attribute(const Attribute&) = default;
    Attribute(Attribute&&) noexcept = default;
    Attribute& operator=(const Attribute&) = delete;
    Attribute& operator=(Attribute&&) = delete;

    constexpr Component& component() noexcept { return _comp; }
    constexpr const Component& component() const noexcept { return _comp; }

    constexpr T& value() noexcept { return _ref; }
    constexpr const T& value() const noexcept { return _ref; }

    Attribute& operator=(const T& v) noexcept(noexcept(_ref = v)) {
        _ref = v;
        return *this;
    }

    /**
     * Отправить текущее зеркальное значение как присвоение целого атрибута NIS (`cmd::assign::Numeric`).
     * Поддерживаются целые типы и `Color` (в линию уходит `raw` как uint32 ≤ 65535).
     * @return false — тип не поддержан, путь не влез в буфер, `tx` занят или ошибка сериализации/записи.
     */
    bool pushAssign(Transceiver& tx) const noexcept {
        char path[kObjPathCap];
        if (!fillObjectPath(path))
            return false;
        char lhs[kObjPathCap];
        {
            const int w = std::snprintf(lhs, sizeof(lhs), "%s.%s", path, Name);
            if (w <= 0 || static_cast<size_t>(w) >= sizeof(lhs))
                return false;
        }

        int32_t wired = 0;
        bool supported = true;
        if constexpr (std::is_same_v<T, bool>) {
            wired = _ref ? 1 : 0;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            wired = _ref;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (_ref > static_cast<uint32_t>(INT32_MAX))
                return false;
            wired = static_cast<int32_t>(_ref);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            wired = static_cast<int32_t>(_ref);
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            wired = static_cast<int32_t>(_ref);
        } else if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t>) {
            wired = static_cast<int32_t>(_ref);
        } else if constexpr (std::is_same_v<T, Color>) {
            wired = static_cast<int32_t>(_ref.raw);
        } else {
            supported = false;
        }

        if (!supported)
            return false;

        const cmd::assign::Numeric cmd(cmd::TargetAttr(lhs), wired);
        return tx.pushCommand(cmd);
    }

    static constexpr const char* attr_name() noexcept { return Name; }
};

} // namespace nex
