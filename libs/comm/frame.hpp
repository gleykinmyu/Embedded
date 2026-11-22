/**
 * @file comm/frame.hpp
 * @brief comm::Frame — hdr + payload.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include "mem/byte_writer.hpp"


namespace comm {

class IFrame {
public: 
    const IFrame * payload = nullptr;
    virtual bool pack(mem::IByteWriter& wr) const = 0;
    virtual bool unpack(const mem::IByteReader& br) = 0;
};

template <typename Hdr>
class Frame : public IFrame {
public:
    using hdr_t = Hdr;
    Hdr hdr{};
};

} // namespace comm
