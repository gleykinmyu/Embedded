/**
 * @file comm/tx_slot.hpp
 * @brief comm::TxSlot — связь физического ресурса с TxJob.
 *
 * Слот не владеет задачей. TxJob живёт у инициатора до onTxDone.
 */

#pragma once

namespace comm {

class TxJob;

struct TxSlot {
    TxJob* job = nullptr;
};

} // namespace comm
