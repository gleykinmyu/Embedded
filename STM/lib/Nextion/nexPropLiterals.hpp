#pragma once

namespace Nextion {
namespace prop {

/** Общие атрибуты (строка — имя в протоколе, без «.»). */
namespace common {

inline constexpr char text[] = "txt";
inline constexpr char value[] = "val";
inline constexpr char font_color[] = "pco";
inline constexpr char bg_color[] = "bco";
inline constexpr char picture[] = "pic";
inline constexpr char picture_alt[] = "pic2";
inline constexpr char text_center_x[] = "xcen";
inline constexpr char text_center_y[] = "ycen";
inline constexpr char font_id[] = "font";
inline constexpr char enabled[] = "en";
inline constexpr char opacity[] = "aph";
inline constexpr char draggable[] = "drag";
inline constexpr char visual_state[] = "sta";
inline constexpr char inner_width[] = "wid";
inline constexpr char height[] = "h";
inline constexpr char width[] = "w";
inline constexpr char pos_x[] = "x";
inline constexpr char pos_y[] = "y";
inline constexpr char var_scope[] = "vscope";
inline constexpr char touch_type[] = "ustype";
inline constexpr char comp_id[] = "id";
inline constexpr char comp_type[] = "type";
inline constexpr char object_name[] = "objname";

} // namespace common

namespace page {

inline constexpr char load_json[] = "loadj";
inline constexpr char send[] = "send";

} // namespace page

namespace text {

inline constexpr char password_mask[] = "pw";
inline constexpr char text_len[] = "length";

} // namespace text

namespace button {

inline constexpr char crop_bg_pic[] = "picc";

} // namespace button

namespace progress {

inline constexpr char fill_dir[] = "dir";

} // namespace progress

namespace slider {

inline constexpr char min_val[] = "minval";
inline constexpr char max_val[] = "maxval";

} // namespace slider

namespace gauge {

inline constexpr char needle_pic[] = "needle";

} // namespace gauge

namespace waveform {

inline constexpr char ch_disable[] = "dis";
inline constexpr char grid_color[] = "gch";
inline constexpr char grid_div[] = "gdc";
inline constexpr char ch0_color[] = "pco0";
inline constexpr char ch1_color[] = "pco1";
inline constexpr char ch2_color[] = "pco2";
inline constexpr char ch3_color[] = "pco3";

} // namespace waveform

namespace timer {

inline constexpr char interval_ms[] = "tim";
inline constexpr char repeat[] = "cyclic";

} // namespace timer

namespace number {

inline constexpr char format[] = "format";
/** В Nextion атрибут назван `lenth`. */
inline constexpr char len_typo[] = "lenth";

} // namespace number

namespace scrolltext {

inline constexpr char text_max_len[] = "txt_maxl";
inline constexpr char scroll_dx[] = "spax";
inline constexpr char scroll_dy[] = "spdy";

} // namespace scrolltext

} // namespace prop
} // namespace Nextion
