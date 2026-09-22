/**
 * @file message_test.cpp
 * @brief Roundtrip черновика PDU-классов. Не вызывается из Session/Node.
 */

#include "smcp/transport/message_test.hpp"

namespace smcp {
namespace msgtest {
namespace {

[[nodiscard]] bool sameNack(const Nack& a, const Nack& b) noexcept
{
    return a.error == b.error && a.detail == b.detail;
}

} // namespace

bool selfCheck() noexcept
{
    Nack nack{0x04u, 0x03u};
    Select sel{1u, 0x00000005u};

    const Pdu* p = &nack;
    if (p->cstr()[0] != 'N') {
        return false;
    }

    msg::Message m{};
    if (!nack.pack(m) || m.id != Nack::kId || m.dlc != 2) {
        return false;
    }

    Nack decoded{};
    if (!decoded.unpack(m) || !sameNack(nack, decoded)) {
        return false;
    }

    if (!sel.pack(m)) {
        return false;
    }
    Select sel2{};
    if (!sel2.unpack(m) || sel2.action != 1u || sel2.mask != 0x00000005u) {
        return false;
    }

    if (decoded.unpack(m)) {
        return false;
    }

    m = {};
    m.id = Select::kId;
    (void)m.push(3u);
    (void)m.pushU32(0);
    Select bad{};
    if (bad.unpack(m)) {
        return false;
    }

    return true;
}

} // namespace msgtest
} // namespace smcp
