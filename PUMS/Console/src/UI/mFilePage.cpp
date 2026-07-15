#include "mFilePage.hpp"

namespace server {

MFilePage::MFilePage(nex::IAppUI& app) noexcept
    : Page<9>(app, HMI_COMP_OBJNAME(mFile), PG::kPageId)
{}

} // namespace server
