/**
 * Хостовая проверка comm::Frame. Контрольные байты заданы заранее.
 */

#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

#include "bitsery/custom/extensions.hpp"
#include "bitsery/custom/msb_bitpack.hpp"
#include "bitsery/custom/serializer.hpp"
#include "comm/frame.hpp"
#include "smcp/new_smcp/proto/base/header.hpp"
#include "smcp/new_smcp/proto/msg/pdu.hpp"

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAIL " << what << '\n';
        ++g_fail;
    }
}

struct Bits3 {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<3>(s, v);
    }
};

struct Bits5 {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<5>(s, v);
    }
};

struct Byte {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<8>(s, v);
    }
};

struct Empty {
    template <typename S>
    void serialize(S&)
    {}
};

struct Word16 {
    uint16_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.value2b(v);
    }
};

struct Word32 {
    uint32_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.value4b(v);
    }
};

struct Track {
    uint8_t v = 0;
    int* copies = nullptr;

    Track() = default;
    Track(const Track& other)
        : v(other.v)
        , copies(other.copies)
    {
        if (copies != nullptr) {
            ++*copies;
        }
    }
    Track& operator=(const Track&) = delete;

    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<8>(s, v);
    }
};

struct NoCopy {
    uint8_t v = 1;
    NoCopy() = default;
    NoCopy(const NoCopy&) = delete;
    template <typename S>
    void serialize(S& s)
    {
        s.value1b(v);
    }
};

struct Wide {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        bitsery::custom::putBits<3>(s, v);
    }
};

template <typename T>
size_t writeOf(const T& obj, uint8_t* buf, size_t cap, size_t& bits, bitsery::custom::msb::WriteError& err)
{
    bitsery::custom::Writer writer{bitsery::custom::msb::OutputAdapter<>{buf, cap}};
    writer.object(obj);
    bits = writer.adapter().bitPos();
    err = writer.adapter().error();
    return writer.adapter().writtenBytesCount();
}

template <typename T>
size_t measureOf(const T& obj, size_t& bits, bitsery::custom::msb::WriteError& err)
{
    bitsery::custom::Measure measured{bitsery::custom::msb::MeasureAdapter<>{}};
    measured.object(obj);
    bits = measured.adapter().bitPos();
    err = measured.adapter().error();
    return measured.adapter().writtenBytesCount();
}

void testCompose()
{
    Bits3 base{};
    base.v = 5;
    Bits5 upper{};
    upper.v = 0x13;
    Byte pdu{};
    pdu.v = 0xA5;

    int copies = 0;
    Track tracked{};
    tracked.v = 0xA5;
    tracked.copies = &copies;

    comm::Frame mid{upper, tracked};
    comm::Frame outer{base, mid};
    check(copies == 0, "payload copied");

    uint8_t buf[8]{};
    size_t bits = 0;
    auto err = bitsery::custom::msb::WriteError::NoError;
    const size_t n = writeOf(outer, buf, sizeof buf, bits, err);
    size_t mbits = 0;
    auto merr = bitsery::custom::msb::WriteError::NoError;
    const size_t mn = measureOf(outer, mbits, merr);

    const uint8_t exp[] = {0xB3, 0xA5};
    check(err == bitsery::custom::msb::WriteError::NoError && merr == err, "compose error");
    check(bits == 16u && mbits == bits && n == mn && n == 2u, "compose size");
    check(std::memcmp(buf, exp, sizeof exp) == 0, "compose bytes");
    check(copies == 0, "payload copied by serialize");
}

void testEmptyAndConst()
{
    Bits3 base{};
    base.v = 5;
    const Empty empty{};
    comm::Frame frame{base, empty};

    uint8_t buf[4]{};
    size_t bits = 0;
    auto err = bitsery::custom::msb::WriteError::NoError;
    const size_t n = writeOf(frame, buf, sizeof buf, bits, err);
    check(err == bitsery::custom::msb::WriteError::NoError, "empty error");
    check(bits == 3u && n == 1u && buf[0] == 0xA0, "empty bytes");

    const Byte body{};
    comm::Frame<Bits3, Byte> view{base, body};
    check(view.payload.v == 0, "const payload");
}

void testEndian()
{
    Word16 hi{};
    hi.v = 0xABCD;
    Word32 lo{};
    lo.v = 0xA1B2C3D4u;
    Word16 top{};
    top.v = 0x1234;
    comm::Frame mid{hi, lo};
    comm::Frame outer{top, mid};

    uint8_t buf[8]{};
    size_t bits = 0;
    auto err = bitsery::custom::msb::WriteError::NoError;
    const size_t n = writeOf(outer, buf, sizeof buf, bits, err);
    const uint8_t exp[] = {0x34, 0x12, 0xCD, 0xAB, 0xD4, 0xC3, 0xB2, 0xA1};
    check(err == bitsery::custom::msb::WriteError::NoError && bits == 64u && n == 8u, "endian size");
    check(std::memcmp(buf, exp, sizeof exp) == 0, "endian bytes");
}

void testRange()
{
    Wide bad{};
    bad.v = 8;
    uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    size_t bits = 0;
    auto err = bitsery::custom::msb::WriteError::NoError;
    (void)writeOf(bad, buf, sizeof buf, bits, err);
    check(err == bitsery::custom::msb::WriteError::InvalidValue && bits == 0u, "range");
    check(buf[0] == 0xFF, "range keeps buffer");
}

void testNoCopyPayload()
{
    Bits3 base{};
    NoCopy pdu{};
    comm::Frame frame{base, pdu};
    uint8_t buf[2]{};
    size_t bits = 0;
    auto err = bitsery::custom::msb::WriteError::NoError;
    (void)writeOf(frame, buf, sizeof buf, bits, err);
    check(err == bitsery::custom::msb::WriteError::NoError && pdu.v == 1, "nocopy");
}

void testSmcp()
{
    using smcp::proto::base::Header;
    using smcp::proto::base::Priority;
    using smcp::proto::base::Proto;
    using smcp::proto::base::addr_t;
    Header base{};
    base.prio = Priority::Max;
    base.dst = addr_t{0x10};
    base.src = addr_t{0x01};
    base.proto = Proto::Message;

    smcp::proto::msg::Header msg{};
    msg.txn = 0x15;
    msg.type = smcp::proto::msg::Type::CtrlMsg;
    smcp::proto::msg::CtrlHeader id{};
    id.id = smcp::proto::msg::CtrlId::Ack;
    const smcp::proto::msg::Ack ack{};

    comm::Frame inner{id, ack};
    comm::Frame mid{msg, inner};
    comm::Frame outer{base, mid};

    uint8_t buf[8]{};
    size_t bits = 0;
    auto err = bitsery::custom::msb::WriteError::NoError;
    const size_t n = writeOf(outer, buf, sizeof buf, bits, err);
    size_t mbits = 0;
    auto merr = bitsery::custom::msb::WriteError::NoError;
    const size_t mn = measureOf(outer, mbits, merr);
    const uint8_t exp[] = {0x08, 0x01, 0xAA, 0x00};
    check(err == bitsery::custom::msb::WriteError::NoError && merr == err, "smcp error");
    check(bits == 29u && mbits == 29u && n == mn && n == 4u, "smcp size");
    check(std::memcmp(buf, exp, 4) == 0, "smcp bytes");
}

using Sample = comm::Frame<Bits3, Byte>;

static_assert(!std::is_polymorphic<Sample>::value, "Frame is static");
static_assert(std::is_constructible<Sample, Bits3, const Byte&>::value, "lvalue payload");
static_assert(!std::is_constructible<Sample, Bits3, Byte>::value, "temporary payload");
static_assert(!std::is_polymorphic<Bits3>::value, "header is static");

} // namespace

int main()
{
    Bits3 base{};
    Byte body{};
    const auto frame = comm::Frame{base, body};
    static_assert(std::is_same<decltype(frame), const comm::Frame<Bits3, Byte>>::value, "ctad");
    (void)frame;

    testCompose();
    testEmptyAndConst();
    testEndian();
    testRange();
    testNoCopyPayload();
    testSmcp();
    if (g_fail != 0) {
        std::cerr << g_fail << " failed\n";
        return 1;
    }
    std::cout << "OK frame\n";
    return 0;
}
