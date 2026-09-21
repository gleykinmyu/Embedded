#pragma once

#include "UI/monitorView.hpp"
#include "UI/nexHmiConfig.hpp"
#include "UI/pages/monitorPage.hpp"
#include "UI/pages/netPage.hpp"
#include "nex.hpp"

namespace ui {

class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    explicit Application(BIF::IByteStream& stream, nex::AppTiming timing) noexcept
        : AppUI(stream, nex::Rect(nex::hmi::kScreenW, nex::hmi::kScreenH), timing)
        , monitor(*this)
        , net(*this)
        , view(*this)
    {
    }

    void boot() noexcept;
    void showMonitor() noexcept;
    void hideMonitor() noexcept;
    void onPageChange(const nex::msg::evPage& e) noexcept override;

    MonitorPage monitor;
    NetPage net;
    MonitorView view;
};

} // namespace ui
