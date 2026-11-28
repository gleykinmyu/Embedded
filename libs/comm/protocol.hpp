/**
 * @file comm/protocol.hpp
 * @brief comm::Protocol — статический уровень над конкретным нижним слоем.
 *
 * Имя Protocol, а не Proto: comm::Proto<Frame> уже занят прежним листом
 * (container.hpp) и используется Node, Session и старыми тестами.
 *
 * requestTx и cancelTx передают тот же TxJob вниз.
 * Кадр собирает конкретный протокол и сразу отдаёт его в lower().send.
 */

#pragma once

#include "comm/tx_job.hpp"

namespace comm {

template <typename Lower>
class Protocol {
public:
    using lower_type = Lower;

    Protocol(const Protocol&) = delete;
    Protocol& operator=(const Protocol&) = delete;

    bool requestTx(TxJob& job) noexcept { return _lower.requestTx(job); }
    bool cancelTx(TxJob& job) noexcept { return _lower.cancelTx(job); }

protected:
    explicit Protocol(Lower& lower) noexcept
        : _lower(lower)
    {}

    [[nodiscard]] Lower& lower() noexcept { return _lower; }

private:
    Lower& _lower;
};

} // namespace comm
