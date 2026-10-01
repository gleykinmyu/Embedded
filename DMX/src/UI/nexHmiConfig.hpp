#pragma once
// HMI DMX-тестер: Nextion Intelligent 4.3" 480×272 landscape.
// monitor — пустая страница под McUI.

#include "syncHMI/nexHmiSync.hpp"

namespace nex {
namespace hmi {

inline constexpr const char* kHmiSource = "NX4827P043_011";
inline constexpr const char* kHmiModel = "Nextion 4.3\" Intelligent 480x272";
inline constexpr uint8_t kPageCount = 1u;
inline constexpr uint16_t kScreenW = 480u;
inline constexpr uint16_t kScreenH = 272u;
inline constexpr bool kHaskeybdAPage = true;
inline constexpr bool kHaskeybdBPage = true;
inline constexpr bool kHaskeybdCPage = false;
inline constexpr uint8_t kKeybdAPageId = 2u;
inline constexpr uint8_t kKeybdBPageId = 3u;

#define HMI_PAGE_monitor(X, ...) \
    X(monitor, 0, ##__VA_ARGS__)

/** Page "monitor" (panel page id 0). Виджетов: 0 — рисует overlay. */
struct Page_monitor {
    static constexpr uint8_t kPageId = 0u;
    static constexpr uint8_t kWidgetCount = 0u;

    enum Id : uint8_t {
        HMI_PAGE_monitor(HMI_ENUM_ITEM)
    };

    static constexpr const char* kNames[] = {
        HMI_PAGE_monitor(HMI_NAME_ITEM)
    };
};

} // namespace hmi
} // namespace nex
