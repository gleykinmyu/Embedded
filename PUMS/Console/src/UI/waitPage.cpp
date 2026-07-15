#include "waitPage.hpp"

namespace server {

WaitPage::WaitPage(nex::IAppUI& app) noexcept
    : Page<0>(app, HMI_COMP_OBJNAME(wait), PG::kPageId)
{}

} // namespace server
