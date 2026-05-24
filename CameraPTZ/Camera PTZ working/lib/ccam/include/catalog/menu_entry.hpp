/**
 * @file menu_entry.hpp
 * @brief Строка каталога subcode-меню (ConvertibleProtocol.pdf v3.05).
 */

#pragma once

#include <cstdint>

namespace ccam::catalog {

struct MenuSubcodeEntry {
    uint8_t subcode;
    const char* name;
};

} // namespace ccam::catalog
