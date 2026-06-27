#pragma once

/**
 * McUI — canvas overlay поверх HMI: `Object`, `Widget`, `Overlay` на `Application`.
 * `Widget::show(overlay)` / `hide` — `sendxy`, `touch.setAllTouchable`, draw.
 */

#include "ovlStyle.hpp"
#include "ovlObject.hpp"
#include "ovlWidget.hpp"
#include "ovlOverlay.hpp"
#include "comp/ovlButton.hpp"
#include "comp/ovlMsgBox.hpp"
