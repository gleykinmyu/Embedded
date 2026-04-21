#pragma once

#include <cstdint>

#include "nexColor.hpp"
#include "nexMessages.hpp"

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
using ClickSimEvent = nex::msg::TouchState;
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
} // namespace nex
