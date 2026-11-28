/**
 * Заголовки base / control / message на одном MSB-first курсоре.
 * Число CAN ID — те же биты [28:0], поток начинается с бита 28.
 */

#include <cstdint>
#include <iostream>

#include "smcp/new_smcp/proto/base/frame.hpp"
#include "smcp/new_smcp/proto/control/frame.hpp"
#include "smcp/new_smcp/proto/msb_bitpack.hpp"
#include "smcp/new_smcp/proto/msg/frame.hpp"

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAIL " << what << "\n";
        ++g_fail;
    }
}

using Out = smcp::proto::msb::OutputAdapter<>;
using In = smcp::proto::msb::InputAdapter<>;

struct BusMsg {
    smcp::proto::base::Header base{};
    smcp::proto::msg::Header msg{};

    template <typename S>
    void serialize(S& s)
    {
        s.object(base);
        s.object(msg);
    }
};

struct BusCtrl {
    smcp::proto::base::Header base{};
    smcp::proto::control::Header ctrl{};

    template <typename S>
    void serialize(S& s)
    {
        s.object(base);
        s.object(ctrl);
    }
};

} // namespace

int main()
{
    BusMsg packet{};
    packet.base.prio = smcp::proto::base::Priority::Max;
    packet.base.dst = smcp::proto::base::addr_t{0x10};
    packet.base.src = smcp::proto::base::addr_t{0x01};
    packet.base.proto = smcp::proto::base::Proto::Message;
    packet.msg.type = smcp::proto::msg::Type::CtrlMsg;
    packet.msg.txn = 0x15;
    packet.msg.id = 0;

    uint8_t buf[8]{};
    bitsery::Serializer<Out> ser{Out{buf, sizeof buf}};
    ser.object(packet);
    check(ser.adapter().error() == smcp::proto::msb::WriteError::NoError, "msg write");
    check(ser.adapter().bitPos() == 29u, "29 bits, no pad between headers");
    const uint8_t exp[] = {0x08, 0x01, 0x95, 0x00};
    check(buf[0] == exp[0] && buf[1] == exp[1] && buf[2] == exp[2] && buf[3] == exp[3], "msg bytes");
    check(smcp::proto::base::msbToInt(buf, 29) == 0x010032A0u, "can id");

    BusMsg back{};
    bitsery::Deserializer<In> des{In{buf, 4}};
    des.object(back);
    check(back.base.prio == smcp::proto::base::Priority::Max, "prio");
    check(back.base.dst.v == 0x10 && back.base.src.v == 0x01, "addr");
    check(back.base.proto == smcp::proto::base::Proto::Message, "proto");
    check(back.msg.type == smcp::proto::msg::Type::CtrlMsg, "type");
    check(back.msg.txn == 0x15 && back.msg.id == 0, "txn id");

    BusCtrl ctrl{};
    ctrl.base = packet.base;
    ctrl.base.proto = smcp::proto::base::Proto::Control;
    ctrl.ctrl.op = smcp::proto::control::Type::Fault;
    uint8_t cbuf[4]{};
    bitsery::Serializer<Out> cser{Out{cbuf, sizeof cbuf}};
    cser.object(ctrl);
    check(cser.adapter().bitPos() == 21u, "base+op");
    check(cbuf[0] == 0x08 && cbuf[1] == 0x01 && cbuf[2] == 0x08, "ctrl bytes");

    if (g_fail != 0) {
        std::cerr << g_fail << " failed\n";
        return 1;
    }
    std::cout << "OK headers\n";
    return 0;
}
