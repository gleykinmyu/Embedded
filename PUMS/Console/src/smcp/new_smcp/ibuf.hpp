/**
 * @file new_smcp/ibuf.hpp
 * @brief Невладеющий буфер тела (Get/Set / MultiFrame).
 */

#pragma once

#include <cstdint>

namespace smcp {

struct IBuf {
    uint8_t* data = nullptr;
    uint16_t size = 0; ///< TX: длина; RX: ёмкость (после приёма — принятые байты в view)
};

} // namespace smcp
