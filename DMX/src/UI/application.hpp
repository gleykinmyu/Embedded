#pragma once

#include "iserial.hpp"
#include "nex.hpp"
#include "UI/graphView.hpp"
#include "UI/monitorView.hpp"
#include "UI/nexHmiConfig.hpp"
#include "UI/pages/monitorPage.hpp"
#include "UI/pages/netPage.hpp"

namespace ui {

class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    static constexpr nex::Baudrate kLinkBaudBoot = nex::Baudrate::b250000;
    static constexpr nex::Baudrate kLinkBaudFast = nex::Baudrate::b921600;

    explicit Application(BIF::IHardwareSerial& link, nex::AppTiming timing) noexcept
        : AppUI(link, nex::Rect(nex::hmi::kScreenW, nex::hmi::kScreenH), timing)
        , monitor(*this)
        , net(*this)
        , view(*this)
        , graph(*this)
        , msgBox(*this)
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
    void onAfterMsgBox(const nex::msg::evMsgBox& e) noexcept override;

    /** После `evPage`: `baud` на панели, затем USART на `kLinkBaudFast`. */
    void applyFastBaudIfNeeded() noexcept;

    MonitorPage monitor;
    NetPage net;
    MonitorView view;
    GraphView graph;
    nex::ovl::MsgBox msgBox;

private:
    BIF::IHardwareSerial& _link;
    bool _wantFastBaud = false;
    bool _fastBaud = false;
    bool _fullRedraw = false;
};

} // namespace ui
