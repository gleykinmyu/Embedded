#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

/** Общие типы Nextion: геометрия, `Color`, `Literal`, `Route`, `BkCmd`, идентификаторы компонентов. */

namespace nex {

// --- Color (RGB565) -----------------------------------------------------------

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
    /** Стандартный цвет NIS (`Color::std`); не `explicit` — неявное преобразование в параметрах `Color`. */
    constexpr Color(std c) noexcept : raw(static_cast<uint16_t>(c)) {}

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

// --- NIS: виджеты / атрибуты (enum class : uint8_t) ---------------------------

/** Атрибут `sta` (фон / заливка): crop | color | image | transparent (0…3). */
enum class BG : uint8_t {
    CropImage   = 0,
    Color       = 1,
    Image       = 2,
    Transparent = 3,
};

/** Режим заливки фона в `xstr` (NIS); те же коды 0…3, что у `BG`. */
using BGStyle = BG;

/** Горизонтальное выравнивание (NIS: `xcen`, 0…2). */
enum class HAlign : uint8_t {
    Left   = 0,
    Center = 1,
    Right  = 2,
};

/** Вертикальное выравнивание (NIS: `ycen`, 0…2). */
enum class VAlign : uint8_t {
    Top    = 0,
    Center = 1,
    Bottom = 2,
};

/** Направление раскрытия списка ComboBox (NIS `dir`). */
enum class ComboExpandDirection : uint8_t {
    Up    = 0u,
    Down  = 1u,
    Left  = 2u,
    Right = 3u,
};

/** Маркер выбранной строки ComboBox (NIS `mode`). */
enum class ComboMarker : uint8_t {
    None       = 0u,
    Triangle   = 1u,
    BgTriangle = 2u,
};

/** Направление прокрутки ScrollText (NIS `dir`). */
enum class ScrollDirection : uint8_t {
    LeftToRight = 0u,
    RightToLeft = 1u,
    UpToDown    = 2u,
    DownToUp    = 3u,
};

/** Индикатор прогресса SlidingText (NIS `left`). */
enum class ShowProgressBar : uint8_t {
    No            = 0u,
    OperationTime = 1u,
    Continuous    = 2u,
};

/** Формат отображения числа (NIS `format`): Decimal, Currency, Hex. */
enum class NumFormat : uint8_t {
    Dec      = 0,
    Currency = 1,
    Hex      = 2,
};

/** Код нажатия/отпускания в touch-событиях и UART `click` (0 / 1). */
enum class TouchState : uint8_t {
    Release = 0x00,
    Press   = 0x01,
};

// --- NIS: протокол / система ---------------------------------------------------

/** NIS §6 `bkcmd`: UART status после команд (0–3, на панели default 2). */
enum class BkCmd : uint8_t {
    Off       = 0,
    OnSuccess = 1,
    OnFailure = 2,
    Always    = 3,
};

/** NIS §6: допустимые значения `baud` / `bauds` (бит/с). */
enum Baudrate : uint32_t {
    b2400   = 2400u,
    b4800   = 4800u,
    b9600   = 9600u,
    b19200  = 19200u,
    b31250  = 31250u,
    b38400  = 38400u,
    b57600  = 57600u,
    b115200 = 115200u,
    b230400 = 230400u,
    b250000 = 250000u,
    b256000 = 256000u,
    b512000 = 512000u,
    b921600 = 921600u,
};

// --- Идентификаторы компонентов и ресурсов ------------------------------------

/**
 * Идентификатор компонента в кадрах touch/status (NIS): `0` — сама страница, не виджет.
 * На панели у объектов `.id` начинается с `kFirstCompId`.
 */
constexpr uint8_t kPageCompId = 0u;

/** Минимальный допустимый `.id` виджета на странице. */
constexpr uint8_t kFirstCompId = 1u;

/** Идентификатор ресурса картинки/страницы в NIS (`pic`, `pic2`, `cpic`, …). */
using PicId = uint16_t;

/** Идентификатор шрифта в NIS (`font`). */
using FontId = uint16_t;

/** Шрифт Nextion: id ресурса и высота глифа в пикселях (как в Font Generator). */
struct Font {
    FontId id = 1u;
    uint16_t heightPx = 16u;

    constexpr Font() noexcept = default;
    constexpr Font(FontId fontId, uint16_t heightPx) noexcept
        : id(fontId)
        , heightPx(heightPx)
    {}

    /** Минимальная ширина подписи: оценка по высоте шрифта + `padX` с каждой стороны. */
    [[nodiscard]] uint16_t minWidthFor(const char* text, uint16_t padX = 12u, int16_t spax = 0) const noexcept;

    /** Минимальная высота под глиф + `padY` сверху и снизу. */
    [[nodiscard]] constexpr uint16_t minHeightFor(uint16_t padY = 8u) const noexcept {
        return static_cast<uint16_t>(heightPx + 2u * padY);
    }
};

// --- Геометрия экрана ---------------------------------------------------------

/** Пиксельная координата X/Y (NIS `x`, `y`, touch): ≥ 0, `uint16_t`, не `int16_t`. */
using Coord = uint16_t;

/** Точка (x, y) на экране в пикселях. */
struct Point {
    Coord x;
    Coord y;

    constexpr Point() noexcept
        : x(0u)
        , y(0u)
    {}
    constexpr Point(Coord px, Coord py) noexcept
        : x(px)
        , y(py)
    {}

    /** Точка правее на `dx` пикселей (тот же `y`). */
    [[nodiscard]] constexpr Point right(Coord dx) const noexcept {
        return Point(static_cast<Coord>(x + dx), y);
    }

    /** Точка левее на `dx` пикселей (тот же `y`, `x` не меньше 0). */
    [[nodiscard]] constexpr Point left(Coord dx) const noexcept {
        return Point(x >= dx ? static_cast<Coord>(x - dx) : 0u, y);
    }
};

/** Размер прямоугольника в пикселях. */
struct Rect {
    uint16_t w = 0u;
    uint16_t h = 0u;

    constexpr Rect() noexcept = default;
    constexpr Rect(uint16_t width, uint16_t height) noexcept
        : w(width)
        , h(height)
    {}
    explicit constexpr Rect(unsigned width, unsigned height) noexcept
        : w(static_cast<uint16_t>(width))
        , h(static_cast<uint16_t>(height))
    {}

    /** `w = (base.w * wNum) / wDen`, `h = (base.h * hNum) / hDen` — беззнаковое целочисленное деление. */
    constexpr Rect(const Rect& base, unsigned wNum, unsigned wDen, unsigned hNum, unsigned hDen) noexcept
        : w(static_cast<uint16_t>(static_cast<unsigned>(base.w) * wNum / wDen))
        , h(static_cast<uint16_t>(static_cast<unsigned>(base.h) * hNum / hDen))
    {}
};

/** Прямоугольная область на экране: верхний левый угол и размер. */
struct Region {
    Point ul;
    Rect size;

    constexpr Region() noexcept = default;
    constexpr Region(Point upperLeft, const Rect& boxSize) noexcept
        : ul(upperLeft)
        , size(boxSize)
    {}

    [[nodiscard]] constexpr Point lowerRight() const noexcept {
        return Point(static_cast<Coord>(ul.x + size.w - 1u), static_cast<Coord>(ul.y + size.h - 1u));
    }

    /** Точка `p` внутри (включительно по границе); при нулевом размере — false. */
    [[nodiscard]] constexpr bool contains(Point p) const noexcept {
        if (size.w == 0u || size.h == 0u)
            return false;
        const Point lr = lowerRight();
        return p.x >= ul.x && p.x <= lr.x && p.y >= ul.y && p.y <= lr.y;
    }

    /** Непустое пересечение с `other`; при нулевом размере у любой — false. */
    [[nodiscard]] constexpr bool overlaps(const Region& other) const noexcept {
        if (size.w == 0u || size.h == 0u || other.size.w == 0u || other.size.h == 0u)
            return false;
        const Point lr = lowerRight();
        const Point otherLr = other.lowerRight();
        return ul.x <= otherLr.x && other.ul.x <= lr.x && ul.y <= otherLr.y && other.ul.y <= lr.y;
    }
};

/** Размеры экрана в пикселях; пользовательские координаты совпадают с координатами экрана. */
struct ScreenLayout {
    Rect size{};

    constexpr ScreenLayout() noexcept = default;
    constexpr ScreenLayout(uint16_t w, uint16_t h) noexcept
        : size(w, h)
    {}
};

// --- Утилиты ------------------------------------------------------------------

/** Меньшее из `a`, `b`. */
template<typename T>
[[nodiscard]] constexpr T min(T a, T b) noexcept {
    return (a < b) ? a : b;
}

/** Ограничить `v` диапазоном `[lo, hi]` (inclusive). */
template<typename T>
[[nodiscard]] constexpr T clamp(T v, T lo, T hi) noexcept {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

// --- Wire: int32 в NIS numeric assign / get -----------------------------------

namespace wire {

template<typename T>
inline constexpr bool is_integral_storage_v = std::disjunction_v<
    std::is_convertible<T, bool>,
    std::is_convertible<T, int8_t>,
    std::is_convertible<T, uint8_t>,
    std::is_convertible<T, int16_t>,
    std::is_convertible<T, uint16_t>,
    std::is_convertible<T, int32_t>>;

template<typename T>
inline constexpr bool is_color_v = std::is_same_v<std::decay_t<T>, Color>;

template<typename T, bool IsEnum = std::is_enum_v<T>>
struct is_u8_enum : std::false_type {};

template<typename T>
struct is_u8_enum<T, true> : std::is_same<std::underlying_type_t<T>, uint8_t> {};

template<typename T>
inline constexpr bool is_u8_enum_v = is_u8_enum<T>::value;

/** `attr::Num` / assign numeric: целые, `Color`, enum class : uint8_t. */
template<typename T>
inline constexpr bool is_attr_numeric_v =
    is_integral_storage_v<T> || is_color_v<T> || is_u8_enum_v<T>;

/** `SysVar<T>`: целые и enum class : uint8_t (без `Color`). */
template<typename T>
inline constexpr bool is_sysvar_numeric_v = is_integral_storage_v<T> || is_u8_enum_v<T>;

template<typename T>
[[nodiscard]] constexpr int32_t toWire(const T& v) noexcept {
    if constexpr (is_color_v<T>) {
        return static_cast<int32_t>(v.raw);
    } else if constexpr (is_u8_enum_v<T>) {
        return static_cast<int32_t>(static_cast<uint8_t>(v));
    } else {
        return static_cast<int32_t>(v);
    }
}

template<typename T>
[[nodiscard]] constexpr T fromWire(int32_t wire) noexcept {
    if constexpr (is_color_v<T>) {
        return Color(static_cast<uint16_t>(wire));
    } else if constexpr (is_u8_enum_v<T>) {
        return static_cast<T>(static_cast<uint8_t>(wire));
    } else {
        return static_cast<T>(wire);
    }
}

} // namespace wire

// --- Строковые литералы и маршруты транзакций ---------------------------------

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

    /** Только для `Page::kNames[i]` из hmi2config (строка в .rodata, `nameLen` без NUL). */
    constexpr Literal(const char* s, uint8_t nameLen) noexcept
        : data(s)
        , len(nameLen)
    {}

    constexpr Literal(const Literal&) noexcept = default;
    Literal(Literal&&) = delete;
    Literal& operator=(const Literal&) = delete;
    Literal& operator=(Literal&&) = delete;
};

/** Пустая лексема (`len == 0`): `AttrRef` без компонента (`sys0`, …), невалидный `attr::Id`, … */
inline constexpr Literal kEmptyLiteral{""};

/**
 * Маршрут транзакции / ошибки / ответа: пара `(page, comp)`.
 * Специальные значения — вне таблицы страниц/компонентов (sysvar, CompIdMap poll, …).
 */
struct Route {
    /** Panel page id; с `comp == 0` — глобальный маршрут `(0,0)` для status вне активной транзакции. */
    uint8_t page = 0u;
    /** Panel component id; `kPageCompId` — касание по странице, не виджет. */
    uint8_t comp = 0u;

    static constexpr uint8_t kSysVarPageId = 0xFFu;
    static constexpr uint8_t kSysVarCompId = 0xFFu;
    static constexpr uint8_t kCompIdMapPollPageId = 0xFEu;
    static constexpr uint8_t kCompIdMapPollCompId = 0xFEu;

    constexpr Route() noexcept = default;
    constexpr Route(uint8_t page, uint8_t comp) noexcept
        : page(page)
        , comp(comp)
    {}

    /** Служебный маршрут зеркал `SysVar` (`0xFF`, `0xFF`). */
    [[nodiscard]] static constexpr Route sysVar() noexcept { return {kSysVarPageId, kSysVarCompId}; }

    [[nodiscard]] static constexpr Route compIdMapPoll() noexcept {
        return {kCompIdMapPollPageId, kCompIdMapPollCompId};
    }

    [[nodiscard]] constexpr bool isSysVar() const noexcept {
        return page == kSysVarPageId && comp == kSysVarCompId;
    }

    [[nodiscard]] constexpr bool isCompIdMapPoll() const noexcept {
        return page == kCompIdMapPollPageId && comp == kCompIdMapPollCompId;
    }

    [[nodiscard]] constexpr bool isGlobal() const noexcept { return page == 0u && comp == 0u; }

    friend constexpr bool operator==(Route a, Route b) noexcept {
        return a.page == b.page && a.comp == b.comp;
    }

    friend constexpr bool operator!=(Route a, Route b) noexcept { return !(a == b); }
};

} // namespace nex
