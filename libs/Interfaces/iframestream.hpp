/**
 * @file iframestream.hpp
 * @brief Кадр = hdr + payload. Поток: send / recv. MTU = sizeof(Frame).
 */
#pragma once

#include <cstddef>

#include "mem/byte_writer.hpp"

namespace BIF {

template <typename Hdr, std::size_t MaxLen>
struct Frame {
    Hdr hdr{};
    mem::ByteBuffer<MaxLen> payload{};
};

template <typename Frame>
class IFrameStream {
public:
    using frame_t = Frame;

    virtual ~IFrameStream() = default;

    [[nodiscard]] virtual bool send(const Frame& frame) noexcept = 0;
    [[nodiscard]] virtual bool recv(Frame& frame) noexcept = 0;
};

} // namespace BIF
