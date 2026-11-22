/**
 * @file comm/message.hpp
 * @brief comm::Message — txn + id + payload.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "mem/byte_writer.hpp"

namespace comm {

template <std::size_t MaxLen, typename Id = uint8_t>
struct Message {
    static constexpr std::size_t kMaxLen = MaxLen;
    using id_type = Id;
    using payload_t = mem::ByteBuffer<MaxLen>;

    uint8_t txn = 0;
    Id id{};
    payload_t payload{};

    [[nodiscard]] std::size_t bodySize() const noexcept { return payload.cursor(); }

    void clear() noexcept
    {
        id = Id{};
        payload.clear();
    }
};

/** Кандидат на удаление: src/dst поверх Message. Нижний кадр — BIF::Frame. */
template <typename Addr, std::size_t Payload, typename Id = uint8_t>
struct Frame : Message<Payload, Id> {
    using addr_t = Addr;
    using id_type = Id;
    using msg_t = Message<Payload, Id>;
    static constexpr std::size_t kMaxPayload = Payload;

    addr_t src{};
    addr_t dst{};

    template <std::size_t N, typename I>
    void copyBody(const Message<N, I>& m) noexcept
    {
        this->id = static_cast<Id>(m.id);
        this->payload.clear();
        (void)this->payload.push(m.payload.data(), m.payload.cursor());
    }
};

} // namespace comm
