#include "mGroupPage.hpp"

namespace server {

MGroupPage::MGroupPage(nex::IAppUI& app) noexcept
    : Page<7>(app, HMI_COMP_OBJNAME(mGroup), PageCfg::kPageId)
{}

} // namespace server
