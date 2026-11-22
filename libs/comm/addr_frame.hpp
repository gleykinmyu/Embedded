/**
 * @file comm/addr_frame.hpp
 * @brief comm::Header — src + dst + prio + proto; addrFrame — BIF::Frame с этим hdr.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "iframestream.hpp"

namespace comm {

template <typename Addr, typename Prio = uint8_t, typename Proto = uint8_t>
struct Header {
    using addr_t = Addr;
    using prio_t = Prio;
    using proto_t = Proto;

    addr_t src{};
    addr_t dst{};
    prio_t prio{};
    proto_t proto{};
};

template <typename Addr, std::size_t MaxLen, typename Prio = uint8_t, typename Proto = uint8_t>
using addrFrame = BIF::Frame<Header<Addr, Prio, Proto>, MaxLen>;

} // namespace comm
