/**
 * @file new_smcp/new_smcp.hpp
 * @brief Новый SMCP на comm: Packet / MsgId / PDU / proto/.
 */
#pragma once

#include "comm/comm.hpp"
#include "smcp/new_smcp/addr.hpp"
#include "smcp/new_smcp/message.hpp"
#include "smcp/new_smcp/packet.hpp"
#include "smcp/new_smcp/pdu.hpp"
#include "smcp/new_smcp/node.hpp"
#include "smcp/new_smcp/session.hpp"
#include "smcp/new_smcp/proto/proto.hpp"
#include "smcp/new_smcp/session_bank.hpp"

namespace smcp {

using ILink = comm::ILink<Packet>;

} // namespace smcp
