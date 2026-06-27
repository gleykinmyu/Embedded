#pragma once

#include "nexApplication.hpp"
#include "../overlay/ovlOverlay.hpp"

namespace nex {

/** `Application` + McUI `Overlay` (`sendxy`, стек `Widget`). */
class OvlApp : public Application {
public:
    ovl::Overlay overlay;

protected:
    OvlApp(BIF::IByteStream& stream, Rect screen, AppTiming timing) noexcept
        : Application(stream, screen, timing)
        , overlay(*this) {}

    void onTouchXY(const msg::evTouchXY& e) override { overlay.dispatchTouchXY(e); }
};

} // namespace nex
