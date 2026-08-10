/**
 * @file nexTimeout.hpp
 * @brief Совместимость: `nex::MsTimer` = `MISC::MsTimer` (см. Interfaces/ms_timer.hpp).
 */

#pragma once

#include "ms_timer.hpp"

namespace nex {

using MsTimer = MISC::MsTimer;

} // namespace nex
