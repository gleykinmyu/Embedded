#include "browserPage.hpp"

namespace server {

BrowserPage::BrowserPage(nex::IAppUI& app) noexcept
    : Page<37>(app, HMI_COMP_OBJNAME(browser), PageCfg::kPageId)
{}

} // namespace server
