/**
 * @file new_smcp/proto/ncp.hpp
 * @brief Проверка адреса на шине — comm::Ncp&lt;Frame&gt;.
 */

#pragma once

#include "comm/node.hpp"
#include "smcp/new_smcp/transport.hpp"

namespace smcp::proto {

using Ncp = comm::Ncp<Frame>;

} // namespace smcp::proto
