/**
 * @file comm/stack.hpp
 * @brief Новый статический стек: Frame, TxJob, Transport, Protocol.
 *
 * В comm.hpp этот заголовок не входит: прежние протоколы не зависят от Bitsery.
 */

#pragma once

#include "comm/frame.hpp"
#include "comm/protocol.hpp"
#include "comm/transport.hpp"
#include "comm/tx_job.hpp"
#include "comm/tx_slot.hpp"
