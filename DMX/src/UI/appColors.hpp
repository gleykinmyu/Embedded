#pragma once

#include "UI/cellMap.hpp"
#include "nex.hpp"
#include "overlay/ovl.hpp"

namespace ui {

using C = nex::Color::std;

inline constexpr nex::Color kBg{C::Black};
inline constexpr nex::Color kChrome{nex::Color(0x2104u)};
inline constexpr nex::Color kCellBg{C::Black};
inline constexpr nex::Color kCellFg{C::Green};
inline constexpr nex::Color kCellGrid{C::Navy};
inline constexpr nex::Color kChangedBg{C::Yellow};
inline constexpr nex::Color kChangedFg{C::Black};
inline constexpr nex::Color kSelect{C::Cyan};
inline constexpr nex::Color kOk{C::Green};
inline constexpr nex::Color kErr{C::Red};
inline constexpr nex::Color kTx{C::Cyan};
inline constexpr nex::Color kText{C::White};
inline constexpr nex::Color kTraceGrid{nex::Color(0x4208u)};

inline constexpr nex::Color kTrace[kMaxSelect]{
    C::Green,
    C::Yellow,
    C::Cyan,
    C::Red,
    C::Magenta,
    C::White,
    C::Orange,
    C::Blue,
};

inline constexpr nex::ovl::ButtonStyle kBtnIdle{
    nex::ovl::TextBoxStyle{nex::Color(0x3186u), C::Gray, 1u, 0u, 24u, C::White},
    nex::ovl::TextBoxStyle{C::Green, C::White, 1u, 0u, 24u, C::Black},
};

inline constexpr nex::ovl::ButtonStyle kBtnOn{
    nex::ovl::TextBoxStyle{C::Green, C::White, 1u, 0u, 24u, C::Black},
    nex::ovl::TextBoxStyle{C::Yellow, C::White, 1u, 0u, 24u, C::Black},
};

} // namespace ui
