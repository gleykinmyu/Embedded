#pragma once

namespace Nextion {
namespace cmd {

inline constexpr char get_attr[] = "get";
inline constexpr char page_open[] = "page";
inline constexpr char screen_brightness[] = "dim";
inline constexpr char sleep[] = "sleep";
inline constexpr char serial_ack_mode[] = "bkcmd";
inline constexpr char send_page_id[] = "sendme";
inline constexpr char refresh[] = "ref";
inline constexpr char refresh_stop[] = "ref_stop";
inline constexpr char click_sim[] = "click";
inline constexpr char visibility[] = "vis";
inline constexpr char touch_switch[] = "tsw";
inline constexpr char trans_begin[] = "tws";
inline constexpr char trans_jump[] = "twjp";
inline constexpr char touch_mode[] = "recmod";
inline constexpr char clear_screen[] = "cls";
inline constexpr char fill_rect[] = "fill";
inline constexpr char wf_add[] = "addt";
inline constexpr char wf_clear_ch[] = "clec";
inline constexpr char do_events[] = "doevents";

} // namespace cmd
} // namespace Nextion
