#pragma once
// HMI DMX-тестер: Nextion Intelligent 10" 1024×600 landscape.
// monitor — пустая страница под McUI; net — поля ввода IP / universe.

#include "syncHMI/nexHmiSync.hpp"

namespace nex {
namespace hmi {

inline constexpr const char* kHmiSource = "NX1060P101_011";
inline constexpr const char* kHmiModel = "Nextion 10.0\" Intelligent 1024x600";
inline constexpr uint8_t kPageCount = 2u;
inline constexpr uint16_t kScreenW = 1024u;
inline constexpr uint16_t kScreenH = 600u;
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

#define HMI_PAGE_net(X, ...) \
    X(net, 0, ##__VA_ARGS__) \
    X(bBack, 1, ##__VA_ARGS__) \
    X(btDhcp, 2, ##__VA_ARGS__) \
    X(nIp0, 3, ##__VA_ARGS__) \
    X(nIp1, 4, ##__VA_ARGS__) \
    X(nIp2, 5, ##__VA_ARGS__) \
    X(nIp3, 6, ##__VA_ARGS__) \
    X(nMask0, 7, ##__VA_ARGS__) \
    X(nMask1, 8, ##__VA_ARGS__) \
    X(nMask2, 9, ##__VA_ARGS__) \
    X(nMask3, 10, ##__VA_ARGS__) \
    X(nGw0, 11, ##__VA_ARGS__) \
    X(nGw1, 12, ##__VA_ARGS__) \
    X(nGw2, 13, ##__VA_ARGS__) \
    X(nGw3, 14, ##__VA_ARGS__) \
    X(nArtNet, 15, ##__VA_ARGS__) \
    X(nArtSub, 16, ##__VA_ARGS__) \
    X(nArtUni, 17, ##__VA_ARGS__) \
    X(nSacnUni, 18, ##__VA_ARGS__) \
    X(nPri, 19, ##__VA_ARGS__) \
    X(tName, 20, ##__VA_ARGS__) \
    X(btMcast, 21, ##__VA_ARGS__) \
    X(tIp, 22, ##__VA_ARGS__) \
    X(tMask, 23, ##__VA_ARGS__) \
    X(tGw, 24, ##__VA_ARGS__) \
    X(tArt, 25, ##__VA_ARGS__) \
    X(tSacn, 26, ##__VA_ARGS__) \
    X(tSrc, 27, ##__VA_ARGS__) \
    X(bOk, 28, ##__VA_ARGS__)

/** Page "net" (panel page id 1). Виджетов: 28. */
struct Page_net {
    static constexpr uint8_t kPageId = 1u;
    static constexpr uint8_t kWidgetCount = 28u;

    enum Id : uint8_t {
        HMI_PAGE_net(HMI_ENUM_ITEM)
    };

    static constexpr const char* kNames[] = {
        HMI_PAGE_net(HMI_NAME_ITEM)
    };
};

} // namespace hmi
} // namespace nex
