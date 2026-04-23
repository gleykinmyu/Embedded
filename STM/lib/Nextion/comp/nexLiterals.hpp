#pragma once

#include <cstdint>

#include "nexColor.hpp"

namespace nex {
namespace cmd {

static_assert(sizeof(nex::Color) == 2u);

// Целые параметры команд — NIS integer; диапазон и знак по смыслу команды (координаты fill обычно 0…размер-1, не полный int32).

inline constexpr char get_attr[] = "get";  // void get(const char *comp_name, const char *attr_name);

inline constexpr char page_open[] = "page";  // void page(uint8_t page_id);

inline constexpr char screen_brightness[] = "dim";  // void dim(uint8_t level /* 0…100 */);

enum class SleepParam : uint8_t {
    Wake  = 0,
    Sleep = 1,
};
inline constexpr char sleep[] = "sleep";  // void sleep(void); void sleep(SleepParam p);

enum class BkcmdMode : uint8_t {
    NoReturn    = 0,
    OnSuccess   = 1,
    Always      = 2,
    NumericOnly = 3,
};
inline constexpr char serial_ack_mode[] = "bkcmd";  // void bkcmd(BkcmdMode mode);

inline constexpr char send_page_id[] = "sendme";  // void sendme(void);

inline constexpr char refresh[] = "ref";  // void ref(const char *component);

inline constexpr char refresh_stop[] = "ref_stop";  // void ref_stop(/* NIS, см. версию */);

/** Второй аргумент `click` — те же значения, что `nex::msg::TouchState` (кадр 0x65, NIS). */

inline constexpr char click_sim[] = "click";  // void click(uint8_t component_id, ClickSimEvent event_id);

enum class VisState : uint8_t {
    Hidden      = 0,
    Visible     = 1,
    GrayNoTouch = 2,
};
inline constexpr char visibility[] = "vis";  // void vis(const char *component, VisState state);

/** Прохождение касаний к компоненту: `On` / `true` — касания включены (NIS: 1). */
enum class TswTouchOn : bool {
    Off = false,
    On  = true,
};
inline constexpr char touch_switch[] = "tsw";  // void tsw(const char *component, TswTouchOn on); // или bool, true == On

inline constexpr char trans_begin[] = "tws";  // void tws(/* int/uint/char* по NIS */);

inline constexpr char trans_jump[] = "twjp";  // void twjp(uint32_t block_id);

enum class TouchRecordMode : uint8_t {
    Passive = 0,
    Active  = 1,
};
inline constexpr char touch_mode[] = "recmod";  // void recmod(TouchRecordMode mode);

inline constexpr char clear_screen[] = "cls";  // void cls(void); void cls(nex::Color color);

inline constexpr char fill_rect[] = "fill";  // void fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, nex::Color c);

inline constexpr char wf_add[] = "addt";  // void addt(/* int/uint по NIS waveform */);

inline constexpr char wf_clear_ch[] = "clec";  // void clec(/* NIS */);

inline constexpr char do_events[] = "doevents";  // void doevents(void);

} // namespace cmd

namespace prop {

    static_assert(sizeof(nex::Color) == 2u);
    
    /**
     * Имена атрибутов Nextion (NIS, без «.»). Типы в `//` — ориентир для MCU; по UART число всё равно строка (десятичная или `0x…` по NIS).
     * Реальный знак и диапазон задаёт компонент и min/max в редакторе: не везде допускаются отрицательные значения и не везде есть смысл в величине > 65535.
     * NumericVariable (`va`, sta=Number) — 32-bit signed (Editor Guide); у слайдера `.val`/`.minval`/`.maxval` в пользовательской доке часто 0…65535; id картинок/шрифтов и координаты пикселей обычно укладываются в uint16_t.
     */
    struct PropNames {
        static constexpr const char text[] = "txt";  // void txt(const char *utf8);
        static constexpr const char value[] = "val";  // int32_t v: NumericVariable(va) число — 32-bit signed (Editor Guide); иначе целое, диапазон по компоненту (слайдер часто 0…65535). Строка — StringVariable: .txt; covx/cov между .txt↔.val.
        static constexpr const char font_color[] = "pco";  // void pco(nex::Color c);
        static constexpr const char bg_color[] = "bco";  // void bco(nex::Color c);
        static constexpr const char picture[] = "pic";  // void pic(uint16_t id);
        static constexpr const char picture_alt[] = "pic2";  // void pic2(uint16_t id);
        static constexpr const char text_center_x[] = "xcen";  // void xcen(bool centered);
        static constexpr const char text_center_y[] = "ycen";  // void ycen(bool centered);
        static constexpr const char font_id[] = "font";  // void font(uint16_t id);
        static constexpr const char enabled[] = "en";  // void en(bool on);
        static constexpr const char opacity[] = "aph";  // void aph(uint8_t level /* 0…127 */);
        static constexpr const char draggable[] = "drag";  // void drag(bool on);
        /** Не один глобальный enum: у текста/кнопок — фон (см. xstr в NIS: 0…3); у va — режим int/str; у других — по типу. */
        static constexpr const char visual_state[] = "sta";  // void sta(uint8_t code);
        static constexpr const char inner_width[] = "wid";  // void wid(uint16_t px /* обычно ≤ размера страницы */);
        static constexpr const char height[] = "h";  // void h(uint16_t px);
        static constexpr const char width[] = "w";  // void w(uint16_t px);
        static constexpr const char pos_x[] = "x";  // void x(uint16_t px /* на экране ≥0 */);
        static constexpr const char pos_y[] = "y";  // void y(uint16_t px);
    
        /**
         * Область видимости имени компонента/переменной в проекте Nextion (NIS): как разрешается путь
         * `b[id].attr` (локально на странице) vs `p[page].b[id].attr` (глобально по страницам).
         * Не относится к «видимости на экране» — для показа/скрытия используются другие атрибуты (`vis`, `aph`, …).
         */
        enum class Vscope : uint8_t {
            Private = 0,
            Global  = 1,
            Static  = 2,
        };
        static constexpr const char var_scope[] = "vscope";  // void vscope(Vscope scope); см. enum Vscope
    
        static constexpr const char touch_type[] = "ustype";  // void ustype(uint8_t code);
        static constexpr const char comp_id[] = "id";  // uint8_t id (часто read-only; см. кадр 0x65);
        static constexpr const char comp_type[] = "type";  // uint8_t type (часто read-only);
        static constexpr const char object_name[] = "objname";  // void objname(const char *name);
    
        struct Page {
            static constexpr const char load_json[] = "loadj";  // void loadj(const char *path);
            static constexpr const char send[] = "send";  // void send(const char *data);
        };
    
        struct Textbox {
            static constexpr const char password_mask[] = "pw";  // void pw(bool masked);
            static constexpr const char text_len[] = "length";  // void length(uint16_t max_chars /* лимит; не везде у Text */);
            /** Лимит длины `.txt`: в Editor — txt-maxl; ITEAD User Guide (Text, Button). */
            static constexpr const char text_max_len[] = "txt_maxl";  // void txt_maxl(uint16_t n);
        };
    
        struct Button {
            static constexpr const char crop_bg_pic[] = "picc";  // void picc(uint16_t id);
        };
    
        struct Progress {
            static constexpr const char fill_dir[] = "dir";  // void dir(uint8_t code);
        };
    
        struct Slider {
            static constexpr const char min_val[] = "minval";  // void minval(uint16_t v /* часто 0…65535 */);
            static constexpr const char max_val[] = "maxval";  // void maxval(uint16_t v);
        };
    
        struct Gauge {
            static constexpr const char needle_pic[] = "needle";  // void needle(uint16_t id);
        };
    
        struct Waveform {
            static constexpr const char ch_disable[] = "dis";  // void dis(uint8_t mask /* NIS */);
            static constexpr const char grid_color[] = "gch";  // void gch(nex::Color c);
            static constexpr const char grid_div[] = "gdc";  // void gdc(uint16_t divisions);
            static constexpr const char ch0_color[] = "pco0";  // void pco0(nex::Color c);
            static constexpr const char ch1_color[] = "pco1";  // void pco1(nex::Color c);
            static constexpr const char ch2_color[] = "pco2";  // void pco2(nex::Color c);
            static constexpr const char ch3_color[] = "pco3";  // void pco3(nex::Color c);
        };
    
        struct Timer {
            static constexpr const char interval_ms[] = "tim";  // void tim(uint32_t ms);
            static constexpr const char repeat[] = "cyclic";  // void cyclic(bool repeat);
        };
    
        struct Number {
            static constexpr const char format[] = "format";  // void format(const char *fmt);
            /** В Nextion имя атрибута `lenth`. */
            static constexpr const char len_typo[] = "lenth";  // void lenth(uint16_t v /* по компоненту */);
        };
    
        struct ScrollText {
            static constexpr const char text_max_len[] = "txt_maxl";  // как у Textbox::text_max_len; void txt_maxl(uint16_t n);
            static constexpr const char scroll_dx[] = "spax";  // void spax(int16_t v /* шаг/скорость, знак по NIS */);
            static constexpr const char scroll_dy[] = "spdy";  // void spdy(int16_t v);
        };
    };
    
    } // namespace prop
} // namespace nex
