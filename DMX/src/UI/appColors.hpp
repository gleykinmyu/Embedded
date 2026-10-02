#pragma once

#include "UI/cellMap.hpp"
#include "nex.hpp"
#include "overlay/ovl.hpp"

namespace ui {

/** Палитра как у PUMS Console (`server::AppColors`, RGB565 из HMI). */
inline constexpr nex::Color kPage{4258u};
inline constexpr nex::Color kDefault{10565u};
inline constexpr nex::Color kMain{64800u};
inline constexpr nex::Color kGroupBlocked{20643u};
inline constexpr nex::Color kServerBlocked{57504u};
inline constexpr nex::Color kBorder{21130u};
inline constexpr nex::Color kText{61277u};
inline constexpr nex::Color kTextLight{65535u};

inline constexpr nex::Color kBg{kPage};
inline constexpr nex::Color kChrome{kPage};
inline constexpr nex::Color kCellBg{kPage};
inline constexpr nex::Color kCellFg{kText};
inline constexpr nex::Color kCellGrid{kBorder};
inline constexpr nex::Color kChangedBg{kServerBlocked};
inline constexpr nex::Color kChangedFg{kTextLight};
inline constexpr nex::Color kSelect{kMain};
inline constexpr nex::Color kOk{kText};
inline constexpr nex::Color kErr{kServerBlocked};
inline constexpr nex::Color kTx{kMain};
inline constexpr nex::Color kTraceGrid{kBorder};

/** Восемь разных линий: по одному оттенку на сектор, без пары жёлтых/розовых. */
inline constexpr nex::Color kTrace[kMaxSelect]{
    kMain,                 // янтарь
    nex::Color(0xE145u),   // красный
    nex::Color(0x264Au),   // зелёный
    nex::Color(0x2594u),   // teal
    nex::Color(0x3A9Cu),   // синий
    nex::Color(0x80FAu),   // фиолет
    nex::Color(0x8A40u),   // шоколад
    kTextLight,            // белый
};

inline constexpr nex::Font kUiFont{0u, 16u};

inline constexpr nex::ovl::ButtonStyle kBtnIdle{
    nex::ovl::TextBoxStyle{kDefault, kBorder, 1u, kUiFont, kText},
    nex::ovl::TextBoxStyle{kMain, kMain, 1u, kUiFont, kTextLight},
};

inline constexpr nex::ovl::ButtonStyle kBtnOn{
    nex::ovl::TextBoxStyle{kMain, kMain, 1u, kUiFont, kTextLight},
    nex::ovl::TextBoxStyle{kMain, kDefault, 1u, kUiFont, kTextLight},
};

inline constexpr nex::ovl::ButtonStyle kBtnSelect{
    nex::ovl::TextBoxStyle{kMain, kTextLight, 1u, kUiFont, kTextLight},
    nex::ovl::TextBoxStyle{kMain, kDefault, 1u, kUiFont, kTextLight},
};

inline constexpr nex::ovl::ButtonStyle kBtnFast{
    nex::ovl::TextBoxStyle{kChangedBg, kTextLight, 1u, kUiFont, kChangedFg},
    nex::ovl::TextBoxStyle{kChangedBg, kDefault, 1u, kUiFont, kChangedFg},
};

inline constexpr nex::ovl::ButtonStyle kBtnDisabled{
    nex::ovl::TextBoxStyle{nex::Color{0u}, kBorder, 1u, kUiFont, kBorder},
    nex::ovl::TextBoxStyle{nex::Color{0u}, kBorder, 1u, kUiFont, kBorder},
};

inline constexpr nex::ovl::MsgBoxColors kAppMsgBoxColors{
    kPage,
    kBorder,
    kTextLight,
    kText,
    kMain,
    kDefault,
    kBorder,
    kText,
    kMain,
    kDefault,
    kMain,
};

} // namespace ui
