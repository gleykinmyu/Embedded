#include "application.hpp"

namespace server {

Application::Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept
    : nex::AppUI<nex::hmi::kPageCount>(stream, screen, timing)
    //, statusBar(screen)
    , wait(*this)
    , work(*this)
    , mGroup(*this)
    , mFile(*this)
    , browser(*this)
{}

void Application::boot() noexcept
{
    //statusBar.show(overlay);
    restartScreen();
    switchPage(0);

}

} // namespace server
