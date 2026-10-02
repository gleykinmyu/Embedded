#pragma once

#include "iserial.hpp"
#include "nex.hpp"
#include "UI/graphView.hpp"
#include "UI/monitorView.hpp"
#include "UI/nexHmiConfig.hpp"
#include "UI/pages/monitorPage.hpp"

namespace ui {

class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    static constexpr nex::Baudrate kLinkBaudBoot = nex::Baudrate::b250000;
    static constexpr nex::Baudrate kLinkBaudFast = nex::Baudrate::b250000;

    explicit Application(BIF::IByteStream& link, nex::AppTiming timing) noexcept
        : AppUI(link, nex::Rect(nex::hmi::kScreenW, nex::hmi::kScreenH), timing)
        , monitor(*this)
        , view(*this)
        , graph(*this)
        , msgBox(*this, kAppMsgBoxColors)
        , _link(link)
    {
    }

    void boot() noexcept;
    void showMonitor() noexcept;
    void hideMonitor() noexcept;
    void showGraph() noexcept;
    void hideGraph() noexcept;
    void refreshUi() noexcept;
    void alert(const char* utf8) noexcept;
    void onPageChange(const nex::msg::evPage& e) noexcept override;
    void onSystemEvent(const nex::msg::evSystem& e) override;
    void onStatus(const nex::msg::Status& status, nex::Route route = {}) noexcept override;
    void onAfterMsgBox(const nex::msg::evMsgBox& e) noexcept override;
    void applyFastBaudIfNeeded() noexcept;

    MonitorPage monitor;
    MonitorView view;
    GraphView graph;
    nex::ovl::MsgBox msgBox;

private:
    bool waitEvPage(uint32_t ms) noexcept;
    bool waitPanelStatus(uint32_t ms) noexcept;
    void showAfterLink() noexcept;

    BIF::IByteStream& _link;
    nex::msg::Status _panelStatus{};
    bool _gotPage = false;
    bool _gotReady = false;
    bool _restSent = false;
    bool _pageAsked = false;
    bool _gotPanelStatus = false;
    bool _linkSettled = false;
    bool _fullRedraw = false;
};

} // namespace ui
