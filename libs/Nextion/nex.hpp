#pragma once

/**
 * Точка входа библиотеки Nextion: `Application` / `IAppUI`, протокол, виджеты `nex::comp`.
 * Подключать один раз из прикладного кода; внутренности — `nexCompImpl.hpp` (не включать напрямую).
 */

#include "core/nexDebug.hpp"
#include "app/nexErrors.hpp"
#include "app/nexApplication.hpp"
#include "comp/nexAppUI.hpp"
#include "app/nexPublicFacades.hpp"
#include "smartApp/nexSmartApp.hpp"
#include "core/nexSession.hpp"
#include "comp/nexAttributes.hpp"
#include "comp/nexComponents.hpp"