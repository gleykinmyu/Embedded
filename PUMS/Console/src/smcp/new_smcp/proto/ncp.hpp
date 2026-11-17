/**
 * @file new_smcp/proto/ncp.hpp
 * @brief Проверка адреса на шине — comm::Ncp&lt;Packet&gt;.
 */

#pragma once

#include "comm/node.hpp"
#include "smcp/new_smcp/packet.hpp"

namespace smcp::proto {

using Ncp = comm::Ncp<Packet>;

} // namespace smcp::proto
