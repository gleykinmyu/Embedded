#pragma once

#include <cstddef>
#include <cstdint>

#include "nexHmiConfig.hpp"
#include "Console/UI/statusBar.hpp"
#include "nex.hpp"

namespace segment {

using namespace nex::comp;

/** Nextion 4.3": page0.cnsl — CLI (UART), StatusBar снизу. */
class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    using AppUI = nex::AppUI<nex::hmi::kPageCount>;

    explicit Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept;

    void boot() noexcept;
    void update() noexcept override;

    void onPageChange(const nex::msg::evPage& e) noexcept override;

private:
    struct ConsolePage : nex::Page<1> {
        HMI_PAGE_CFG(page0);
        /**
         * HMI (Server 1.HMI, объект `cnsl`): обязательно Touch Press Event и
         * Touch Release Event («Send Component ID»). Иначе follow не увидит 0x65.
         */
        HMI_COMP(SlidingLog<>, cnsl);

        explicit ConsolePage(nex::IAppUI& app) noexcept;
    };

    /** Высота `cnsl` в Server 1.HMI; бар — остаток экрана. */
    static constexpr nex::Coord kCnslHeight = 230;

    static void onSerial1Log(const char* data, std::size_t len) noexcept;

    void syncStatusBarLink() noexcept;
    void syncStatusBarTime() noexcept;

    ConsolePage _page;
    server::StatusBar _statusBar;
    uint32_t _statusBarTickMs = 0u;
};

} // namespace segment

extern segment::Application app;
