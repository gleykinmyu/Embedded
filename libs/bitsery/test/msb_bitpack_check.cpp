/**
 * Хостовая проверка MSB-first адаптера. Эталоны байтов заданы явно.
 * Матрица смещений сверяется с побитовой моделью, отдельно проверенной на примере из контракта.
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <bitsery/ext/value_range.h>
#include <bitsery/traits/array.h>
#include <bitsery/traits/vector.h>

#include "bitsery/custom/extensions.hpp"
#include "bitsery/custom/msb_bitpack.hpp"

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAIL " << what << "\n";
        ++g_fail;
    }
}

void expectBytes(const uint8_t* got, size_t n, const uint8_t* exp, const char* what)
{
    if (std::memcmp(got, exp, n) != 0) {
        std::cerr << "FAIL " << what << " bytes";
        for (size_t i = 0; i < n; ++i) {
            std::cerr << ' ' << std::hex << int(got[i]);
        }
        std::cerr << "\n";
        ++g_fail;
    }
}

/** Независимая модель: один бит за раз, старший бит поля первым. */
void refWrite(uint8_t* buf, size_t& bitPos, uint64_t value, size_t bits)
{
    if (bits < 64u) {
        value &= (uint64_t{1} << bits) - 1u;
    }
    for (size_t i = 0; i < bits; ++i) {
        const size_t shift = bits - 1u - i;
        const unsigned bit = static_cast<unsigned>((value >> shift) & 1u);
        const size_t index = bitPos / 8u;
        const unsigned dest = 7u - static_cast<unsigned>(bitPos % 8u);
        if (bitPos % 8u == 0u) {
            buf[index] = 0;
        }
        buf[index] = static_cast<uint8_t>(buf[index] | (bit << dest));
        ++bitPos;
    }
}

using Out = bitsery::custom::msb::OutputAdapter<>;
using In = bitsery::custom::msb::InputAdapter<>;

struct BeConfig : bitsery::DefaultConfig {
    static constexpr bitsery::EndiannessType Endianness = bitsery::EndiannessType::BigEndian;
};

void testSpecPatterns()
{
    uint8_t buf[4]{};
    Out out{buf, sizeof buf};
    out.writeBits(uint8_t{5}, 3);
    out.writeBits(uint16_t{0x1234}, 16);
    out.writeBits(uint8_t{19}, 5);
    const uint8_t exp[] = {0xA2, 0x46, 0x93};
    check(out.error() == bitsery::custom::msb::WriteError::NoError, "spec write err");
    check(out.bitPos() == 24u, "spec bits");
    expectBytes(buf, 3, exp, "spec");

    size_t pos = 0;
    uint8_t ref[4]{};
    refWrite(ref, pos, 5, 3);
    refWrite(ref, pos, 0x1234, 16);
    refWrite(ref, pos, 19, 5);
    expectBytes(ref, 3, exp, "ref matches spec");

    In in{buf, 3};
    uint8_t flags = 0;
    uint16_t value = 0;
    uint8_t type = 0;
    in.readBits(flags, 3);
    in.readBits(value, 16);
    in.readBits(type, 5);
    check(flags == 5 && value == 0x1234 && type == 19, "spec read");
    check(in.error() == bitsery::ReaderError::NoError, "spec read err");
    check(in.isCompletedSuccessfully(), "spec complete");
}

void testAbcLayout()
{
    uint8_t buf[2]{};
    Out out{buf, 2};
    out.writeBits(uint8_t{0x5}, 3);
    out.writeBits(uint8_t{0x15}, 7);
    out.writeBits(uint8_t{0x2A}, 6);
    const uint8_t exp[] = {0xA5, 0x6A};
    expectBytes(buf, 2, exp, "abc");
    check(out.bitPos() == 16u, "abc bits");
    check(out.writtenBytesCount() == 2u, "abc bytes");
}

void testWidthsAndLimits()
{
    uint8_t buf[16]{};
    Out out{buf, sizeof buf};
    out.writeBits(uint8_t{1}, 1);
    out.writeBits(uint8_t{0xFF}, 8);
    out.writeBits(uint16_t{0xFFFF}, 16);
    out.writeBits(uint32_t{0xFFFFFFFFu}, 32);
    out.writeBits(uint64_t{0xFFFFFFFFFFFFFFFFull}, 64);
    check(out.error() == bitsery::custom::msb::WriteError::NoError, "limits write");

    In in{buf, out.writtenBytesCount()};
    uint8_t a = 0;
    uint8_t b = 0;
    uint16_t c = 0;
    uint32_t d = 0;
    uint64_t e = 0;
    in.readBits(a, 1);
    in.readBits(b, 8);
    in.readBits(c, 16);
    in.readBits(d, 32);
    in.readBits(e, 64);
    check(a == 1 && b == 0xFF && c == 0xFFFF && d == 0xFFFFFFFFu && e == ~uint64_t{0}, "limits read");

    const uint8_t be64[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t full[8]{};
    Out wide{full, 8};
    wide.writeBits(~uint64_t{0}, 64);
    expectBytes(full, 8, be64, "u64 full");

    uint8_t full32[4]{};
    Out w32{full32, 4};
    w32.writeBits(uint32_t{0x12345678u}, 32);
    const uint8_t exp32[] = {0x12, 0x34, 0x56, 0x78};
    expectBytes(full32, 4, exp32, "u32 full msb");
}

void testEndianSplit()
{
    uint8_t bits[2]{};
    Out bitOut{bits, 2};
    bitOut.writeBits(uint16_t{0x1234}, 16);
    const uint8_t msb[] = {0x12, 0x34};
    expectBytes(bits, 2, msb, "writeBits 16");

    uint8_t le[2]{};
    Out leOut{le, 2};
    bitsery::Serializer<Out> ser{leOut};
    const uint16_t value = 0x1234;
    ser.value2b(value);
    const uint8_t leExp[] = {0x34, 0x12};
    expectBytes(le, 2, leExp, "value2b le");

    uint8_t be[2]{};
    using BeOut = bitsery::custom::msb::OutputAdapter<BeConfig>;
    BeOut beOut{be, 2};
    bitsery::Serializer<BeOut> beSer{beOut};
    beSer.value2b(value);
    expectBytes(be, 2, msb, "value2b be");

    uint8_t shifted[4]{};
    Out sh{shifted, 4};
    sh.writeBits(uint8_t{0}, 3);
    bitsery::Serializer<Out> shSer{sh};
    shSer.value2b(value);
    const uint8_t shExp[] = {0x06, 0x82, 0x40};
    expectBytes(shifted, 3, shExp, "value2b le off 3");
}

void testOffsetMatrix()
{
    const uint64_t samples[] = {0u, 1u, 0x1234u, 0x12345678u, 0x0123456789ABCDEFull, ~uint64_t{0}};
    const size_t widths[] = {8u, 16u, 32u, 64u};
    for (int offset = 0; offset <= 7; ++offset) {
        for (size_t wi = 0; wi < 4; ++wi) {
            const size_t width = widths[wi];
            for (uint64_t sample : samples) {
                if (width < 64u) {
                    sample &= (uint64_t{1} << width) - 1u;
                }
                uint8_t got[24]{};
                uint8_t exp[24]{};
                size_t expPos = 0;
                if (offset > 0) {
                    refWrite(exp, expPos, 0, static_cast<size_t>(offset));
                }
                refWrite(exp, expPos, sample, width);

                Out out{got, sizeof got};
                if (offset > 0) {
                    out.writeBits(uint8_t{0}, static_cast<size_t>(offset));
                }
                if (width == 8u) {
                    out.writeBits(static_cast<uint8_t>(sample), 8);
                } else if (width == 16u) {
                    out.writeBits(static_cast<uint16_t>(sample), 16);
                } else if (width == 32u) {
                    out.writeBits(static_cast<uint32_t>(sample), 32);
                } else {
                    out.writeBits(sample, 64);
                }
                const size_t nbytes = (static_cast<size_t>(offset) + width + 7u) / 8u;
                expectBytes(got, nbytes, exp, "offset writeBits");
                check(out.error() == bitsery::custom::msb::WriteError::NoError, "offset write err");

                In in{got, nbytes};
                if (offset > 0) {
                    uint8_t skip = 0;
                    in.readBits(skip, static_cast<size_t>(offset));
                }
                uint64_t back = 0;
                if (width == 8u) {
                    uint8_t v = 0;
                    in.readBits(v, 8);
                    back = v;
                } else if (width == 16u) {
                    uint16_t v = 0;
                    in.readBits(v, 16);
                    back = v;
                } else if (width == 32u) {
                    uint32_t v = 0;
                    in.readBits(v, 32);
                    back = v;
                } else {
                    in.readBits(back, 64);
                }
                check(back == sample, "offset readBits");
            }
        }
    }
}

void testValueAtOffsets()
{
    const uint32_t sample = 0xA1B2C3D4u;
    for (int offset = 0; offset <= 7; ++offset) {
        for (int big = 0; big < 2; ++big) {
            uint8_t buf[8]{};
            if (big == 0) {
                Out out{buf, sizeof buf};
                if (offset > 0) {
                    out.writeBits(uint8_t{0}, static_cast<size_t>(offset));
                }
                bitsery::Serializer<Out> ser{out};
                ser.value4b(sample);
                In in{buf, ser.adapter().writtenBytesCount()};
                if (offset > 0) {
                    uint8_t skip = 0;
                    in.readBits(skip, static_cast<size_t>(offset));
                }
                bitsery::Deserializer<In> des{in};
                uint32_t back = 0;
                des.value4b(back);
                check(back == sample, "value4b le offset");
                uint8_t native[4]{};
                std::memcpy(native, &sample, 4);
                uint8_t exp[8]{};
                size_t pos = 0;
                if (offset > 0) {
                    refWrite(exp, pos, 0, static_cast<size_t>(offset));
                }
                for (uint8_t byte : native) {
                    refWrite(exp, pos, byte, 8);
                }
                expectBytes(buf, (static_cast<size_t>(offset) + 32u + 7u) / 8u, exp, "value4b le bytes");
            } else {
                using BeOut = bitsery::custom::msb::OutputAdapter<BeConfig>;
                using BeIn = bitsery::custom::msb::InputAdapter<BeConfig>;
                BeOut out{buf, sizeof buf};
                if (offset > 0) {
                    out.writeBits(uint8_t{0}, static_cast<size_t>(offset));
                }
                bitsery::Serializer<BeOut> ser{out};
                ser.value4b(sample);
                BeIn in{buf, ser.adapter().writtenBytesCount()};
                if (offset > 0) {
                    uint8_t skip = 0;
                    in.readBits(skip, static_cast<size_t>(offset));
                }
                bitsery::Deserializer<BeIn> des{in};
                uint32_t back = 0;
                des.value4b(back);
                check(back == sample, "value4b be offset");
                uint8_t native[4] = {0xA1, 0xB2, 0xC3, 0xD4};
                uint8_t exp[8]{};
                size_t pos = 0;
                if (offset > 0) {
                    refWrite(exp, pos, 0, static_cast<size_t>(offset));
                }
                for (uint8_t byte : native) {
                    refWrite(exp, pos, byte, 8);
                }
                expectBytes(buf, (static_cast<size_t>(offset) + 32u + 7u) / 8u, exp, "value4b be bytes");
            }
        }
    }
}

struct Bits3 {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.ext(v, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{7}});
    }
};

struct Bits7 {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.enableBitPacking([&](typename S::BPEnabledType& bp) {
            bp.ext(v, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{127}});
        });
    }
};

struct Bits6 {
    uint8_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.ext(v, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{63}});
    }
};

struct Chain {
    Bits3 a{};
    Bits7 b{};
    Bits6 c{};
    template <typename S>
    void serialize(S& s)
    {
        s.object(a);
        s.object(b);
        s.object(c);
    }
};

void testObjectsAndMeasure()
{
    Chain chain{};
    chain.a.v = 5;
    chain.b.v = 0x15;
    chain.c.v = 0x2A;
    uint8_t buf[4]{};
    using Ser = bitsery::Serializer<Out>;
    Ser ser{Out{buf, sizeof buf}};
    ser.object(chain);
    check(ser.adapter().bitPos() == 16u, "chain no per-object align");
    const uint8_t exp[] = {0xA5, 0x6A};
    expectBytes(buf, 2, exp, "chain");

    bitsery::custom::msb::MeasureAdapter<> measure;
    bitsery::Serializer<bitsery::custom::msb::MeasureAdapter<>> mser{measure};
    mser.object(chain);
    check(mser.adapter().bitPos() == 16u, "measure bits");
    check(mser.adapter().writtenBytesCount() == 2u, "measure bytes");
    mser.adapter().flush();
    check(mser.adapter().writtenBytesCount() == ser.adapter().writtenBytesCount(), "measure == actual");

    In in{buf, 2};
    Chain back{};
    bitsery::Deserializer<In> des{in};
    des.object(back);
    check(back.a.v == 5 && back.b.v == 0x15 && back.c.v == 0x2A, "chain read");
}

void testBoolRangeAlign()
{
    uint8_t buf[4]{};
    Out out{buf, sizeof buf};
    bitsery::Serializer<Out> ser{out};
    ser.boolValue(true);
    ser.boolValue(false);
    ser.boolValue(true);
    const uint8_t expBits = 0b10100000;
    check(buf[0] == expBits, "bools");

    uint8_t ranged = 10;
    ser.ext(ranged, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{10}});
    check(ser.adapter().bitPos() == 3u + 4u, "range bits");

    ser.adapter().align();
    check(ser.adapter().bitPos() == 8u, "align once");
    ser.adapter().align();
    check(ser.adapter().bitPos() == 8u, "align aligned");

    In in{buf, 1};
    bitsery::Deserializer<In> des{in};
    bool a = false;
    bool b = true;
    bool c = false;
    des.boolValue(a);
    des.boolValue(b);
    des.boolValue(c);
    check(a && !b && c, "bool read");
    uint8_t rv = 0;
    des.ext(rv, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{10}});
    check(rv == 10, "range read");
    des.adapter().align();
    check(des.adapter().error() == bitsery::ReaderError::NoError, "zero pad");
}

void testBadRangeAndPad()
{
    uint8_t buf[1] = {0b10110000};
    In in{buf, 1};
    bitsery::Deserializer<In> des{in};
    uint8_t ranged = 0;
    des.ext(ranged, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{10}});
    check(des.adapter().error() == bitsery::ReaderError::InvalidData, "range 11");
    const auto first = des.adapter().error();
    des.adapter().error(bitsery::ReaderError::DataOverflow);
    check(des.adapter().error() == first, "sticky");

    uint8_t padded[1] = {0b10100001};
    In pad{padded, 1};
    uint8_t top = 0;
    pad.readBits(top, 3);
    pad.align();
    check(pad.error() == bitsery::ReaderError::InvalidData, "nonzero pad");
}

void testOverflow()
{
    uint8_t one[1]{};
    Out out{one, 1};
    out.writeBits(uint32_t{0xFFFFFFFFu}, 32);
    check(out.error() == bitsery::custom::msb::WriteError::BufferOverflow, "short out");
    out.writeBits(uint8_t{1}, 1);
    check(out.error() == bitsery::custom::msb::WriteError::BufferOverflow, "sticky write");

    uint8_t tiny[1] = {0xFF};
    In in{tiny, 1};
    uint32_t value = 0;
    in.readBits(value, 32);
    check(in.error() == bitsery::ReaderError::DataOverflow, "short in");
    check(!in.isCompletedSuccessfully(), "no false success");

    uint8_t cross[1] = {0xAB};
    In edge{cross, 1};
    uint16_t wide = 0;
    edge.readBits(wide, 16);
    check(edge.error() == bitsery::ReaderError::DataOverflow, "cross end");
}

void testBytesMixAndReuse()
{
    uint8_t buf[8]{};
    Out out{buf, sizeof buf};
    out.writeBits(uint8_t{0x7}, 3);
    const uint8_t raw[] = {0x11, 0x22};
    out.writeBuffer<1>(raw, 2);
    out.writeBits(uint8_t{0x1}, 1);
    check(out.error() == bitsery::custom::msb::WriteError::NoError, "mix");

    In in{buf, out.writtenBytesCount()};
    uint8_t top = 0;
    in.readBits(top, 3);
    uint8_t back[2]{};
    in.readBuffer<1>(back, 2);
    uint8_t tail = 0;
    in.readBits(tail, 1);
    check(top == 7 && back[0] == 0x11 && back[1] == 0x22 && tail == 1, "mix read");

    uint8_t again[1]{};
    out.reset(again, 1);
    out.writeBits(uint8_t{0xA5}, 8);
    check(again[0] == 0xA5 && out.error() == bitsery::custom::msb::WriteError::NoError, "reuse");
}

void testSignedEnumFloat()
{
    const int16_t n = -2;
    const int32_t w = -1000;
    enum class Code : uint16_t { A = 1, B = 0x1234 };
    const Code code = Code::B;
    const float f = 1.0f;
    const double d = -2.5;

    uint8_t buf[32]{};
    Out out{buf, sizeof buf};
    bitsery::Serializer<Out> ser{out};
    ser.value2b(n);
    ser.value4b(w);
    ser.value2b(code);
    ser.value4b(f);
    ser.value8b(d);

    int16_t n2 = 0;
    int32_t w2 = 0;
    Code code2 = Code::A;
    float f2 = 0;
    double d2 = 0;
    In in{buf, ser.adapter().writtenBytesCount()};
    bitsery::Deserializer<In> des{in};
    des.value2b(n2);
    des.value4b(w2);
    des.value2b(code2);
    des.value4b(f2);
    des.value8b(d2);
    check(n2 == n && w2 == w && code2 == code, "signed enum");
    check(f2 == f && d2 == d, "float values");

    uint32_t fbits = 0;
    uint64_t dbits = 0;
    std::memcpy(&fbits, &f, 4);
    std::memcpy(&dbits, &d, 8);
    uint8_t exp[32]{};
    size_t pos = 0;
    auto pushLe = [&](uint64_t v, size_t nbytes) {
        for (size_t i = 0; i < nbytes; ++i) {
            refWrite(exp, pos, (v >> (8u * i)) & 0xFFu, 8);
        }
    };
    pushLe(static_cast<uint16_t>(n), 2);
    pushLe(static_cast<uint32_t>(w), 4);
    pushLe(static_cast<uint16_t>(code), 2);
    pushLe(fbits, 4);
    pushLe(dbits, 8);
    expectBytes(buf, pos / 8u, exp, "float bits le");
}

struct Item {
    uint16_t v = 0;
    template <typename S>
    void serialize(S& s)
    {
        s.value2b(v);
    }
};

struct Mixed {
    uint8_t flag = 1;
    uint16_t word = 0x1234;
    std::array<Item, 2> raw{{{0xAB}, {0xCD}}};
    Bits3 tail{};

    template <typename S>
    void serialize(S& s)
    {
        s.ext(flag, bitsery::ext::ValueRange<uint8_t>{uint8_t{0}, uint8_t{1}});
        s.value2b(word);
        s.container(raw);
        s.object(tail);
        s.adapter().align();
    }
};

void testMeasureMixed()
{
    Mixed mixed{};
    mixed.tail.v = 5;
    uint8_t buf[16]{};
    bitsery::Serializer<Out> ser{Out{buf, sizeof buf}};
    ser.object(mixed);
    const size_t actual = ser.adapter().writtenBytesCount();

    bitsery::Serializer<bitsery::custom::msb::MeasureAdapter<>> mser{bitsery::custom::msb::MeasureAdapter<>{}};
    mser.object(mixed);
    check(mser.adapter().writtenBytesCount() == actual, "mixed measure");
    check(mser.adapter().bitPos() == ser.adapter().bitPos(), "mixed bits");
}

void testZeroWidthAndReject()
{
    uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    Out out{buf, sizeof buf};
    out.writeBits(uint8_t{0}, 0);
    check(out.error() == bitsery::custom::msb::WriteError::NoError && out.bitPos() == 0u, "zero bits");
    out.writeBits(uint8_t{1}, 0);
    check(out.error() == bitsery::custom::msb::WriteError::InvalidValue && out.bitPos() == 0u, "zero trunc");
    check(buf[0] == 0xFF, "zero no write");

    out.reset(buf, sizeof buf);
    buf[0] = 0;
    out.writeBits(uint8_t{0x1F}, 3);
    check(out.error() == bitsery::custom::msb::WriteError::InvalidValue && out.bitPos() == 0u && buf[0] == 0,
          "no trunc");
    out.writeBits(uint8_t{1}, 1);
    check(out.error() == bitsery::custom::msb::WriteError::InvalidValue && out.bitPos() == 0u, "sticky invalid");

    bitsery::custom::msb::MeasureAdapter<> measure;
    measure.writeBits(uint8_t{0}, 0);
    check(measure.bitPos() == 0u && measure.error() == bitsery::custom::msb::WriteError::NoError, "measure zero");
    measure.writeBits(uint8_t{0x1F}, 3);
    check(measure.error() == bitsery::custom::msb::WriteError::InvalidValue && measure.bitPos() == 0u,
          "measure no trunc");
    measure.writeBits(uint8_t{1}, 8);
    check(measure.bitPos() == 0u && measure.error() == bitsery::custom::msb::WriteError::InvalidValue,
          "measure sticky");
    measure.reset();
    measure.writeBits(uint8_t{1}, 3);
    check(measure.bitPos() == 3u && measure.error() == bitsery::custom::msb::WriteError::NoError, "measure reset");

    out.reset(buf, sizeof buf);
    bitsery::Serializer<Out> ser{out};
    uint8_t only = 5;
    ser.ext(only, bitsery::ext::ValueRange<uint8_t>{uint8_t{5}, uint8_t{5}});
    check(ser.adapter().error() == bitsery::custom::msb::WriteError::NoError && ser.adapter().bitPos() == 0u,
          "range zero");
    In in{buf, 1};
    uint8_t back = 0;
    bitsery::Deserializer<In> des{in};
    des.ext(back, bitsery::ext::ValueRange<uint8_t>{uint8_t{5}, uint8_t{5}});
    check(back == 5 && des.adapter().error() == bitsery::ReaderError::NoError, "range zero read");

    uint8_t addr = 0x80;
    bitsery::custom::Writer writer{bitsery::custom::msb::OutputAdapter<>{buf, sizeof buf}};
    bitsery::custom::putBits(writer, addr, uint8_t{0x7F});
    check(writer.adapter().error() == bitsery::custom::msb::WriteError::InvalidValue
              && writer.adapter().bitPos() == 0u,
          "putBits");
    bitsery::custom::Measure measured{bitsery::custom::msb::MeasureAdapter<>{}};
    bitsery::custom::putBits(measured, addr, uint8_t{0x7F});
    check(measured.adapter().error() == bitsery::custom::msb::WriteError::InvalidValue
              && measured.adapter().bitPos() == 0u,
          "putBits measure");
}

void testReadEnd()
{
    uint8_t buf[4] = {0xA2, 0x46, 0x93, 0xFF};
    In in{buf, sizeof buf};
    in.currentReadEndPos(3);
    check(in.currentReadEndPos() == 3u, "end pos");
    check(in.bitsRemaining() == 24u, "end remaining");
    uint8_t flags = 0;
    uint16_t value = 0;
    uint8_t type = 0;
    in.readBits(flags, 3);
    in.readBits(value, 16);
    in.readBits(type, 5);
    check(flags == 5 && value == 0x1234 && type == 19, "end read");
    check(in.bitsRemaining() == 0u, "end consumed");
    check(in.isCompletedSuccessfully(), "end complete ignores tail");
    uint8_t extra = 0;
    in.readBits(extra, 1);
    check(in.error() == bitsery::ReaderError::DataOverflow, "past end");
    const auto first = in.error();
    in.error(bitsery::ReaderError::InvalidData);
    check(in.error() == first, "end sticky");

    In cut{buf, sizeof buf};
    cut.currentReadEndPos(1);
    check(cut.bitsRemaining() == 8u, "one byte window");
    uint16_t wide = 1;
    cut.readBits(wide, 16);
    check(cut.error() == bitsery::ReaderError::DataOverflow && cut.bitPos() == 0u, "no partial read");
    check(wide == 0, "failed read cleared");
    check(buf[3] == 0xFF, "buffer stays");

    cut.reset(buf, 1);
    check(cut.error() == bitsery::ReaderError::NoError && cut.bitsRemaining() == 8u, "input reset");
    cut.currentReadEndPos(4);
    check(cut.error() == bitsery::ReaderError::DataOverflow, "end past cap");
}

void testPutBitsWidth()
{
    enum class Code : uint8_t { Off = 0, On = 1 };

    uint8_t buf[16]{};
    bitsery::custom::Writer writer{bitsery::custom::msb::OutputAdapter<>{buf, sizeof buf}};
    uint8_t three = 5;
    uint16_t wide = 0x1234;
    uint32_t full32 = 0xA1B2C3D4u;
    uint64_t full64 = 0x0123456789ABCDEFull;
    Code code = Code::On;
    bitsery::custom::putBits<3>(writer, three);
    bitsery::custom::putBits<16>(writer, wide);
    bitsery::custom::putBits<3>(writer, code);
    bitsery::custom::putBits<32>(writer, full32);
    bitsery::custom::putBits<64>(writer, full64);
    check(writer.adapter().error() == bitsery::custom::msb::WriteError::NoError, "putBits write");
    check(writer.adapter().bitPos() == 3u + 16u + 3u + 32u + 64u, "putBits width");

    const uint8_t exp[] = {
        0xA2, 0x46, 0x86, 0x86, 0xCB, 0x0F, 0x50,
        0x04, 0x8D, 0x15, 0x9E, 0x26, 0xAF, 0x37, 0xBC,
    };
    expectBytes(buf, sizeof exp, exp, "putBits bytes");

    bitsery::custom::Measure measured{bitsery::custom::msb::MeasureAdapter<>{}};
    bitsery::custom::putBits<3>(measured, three);
    bitsery::custom::putBits<16>(measured, wide);
    bitsery::custom::putBits<3>(measured, code);
    bitsery::custom::putBits<32>(measured, full32);
    bitsery::custom::putBits<64>(measured, full64);
    check(measured.adapter().bitPos() == writer.adapter().bitPos(), "putBits measure");
    check(measured.adapter().writtenBytesCount() == writer.adapter().writtenBytesCount(), "putBits size");

    uint8_t back3 = 0;
    uint16_t back16 = 0;
    Code backCode = Code::Off;
    uint32_t back32 = 0;
    uint64_t back64 = 0;
    bitsery::custom::Reader reader{bitsery::custom::msb::InputAdapter<>{buf, sizeof exp}};
    bitsery::custom::putBits<3>(reader, back3);
    bitsery::custom::putBits<16>(reader, back16);
    bitsery::custom::putBits<3>(reader, backCode);
    bitsery::custom::putBits<32>(reader, back32);
    bitsery::custom::putBits<64>(reader, back64);
    check(back3 == 5 && back16 == 0x1234 && backCode == Code::On, "putBits read");
    check(back32 == full32 && back64 == full64, "putBits full");

    uint8_t bad = 8;
    bitsery::custom::Writer reject{bitsery::custom::msb::OutputAdapter<>{buf, sizeof buf}};
    bitsery::custom::putBits<3>(reject, bad);
    check(reject.adapter().error() == bitsery::custom::msb::WriteError::InvalidValue, "putBits reject");
    check(reject.adapter().bitPos() == 0u, "putBits no trunc");
    bitsery::custom::Measure rejectM{bitsery::custom::msb::MeasureAdapter<>{}};
    bitsery::custom::putBits<3>(rejectM, bad);
    check(rejectM.adapter().error() == bitsery::custom::msb::WriteError::InvalidValue
              && rejectM.adapter().bitPos() == 0u,
          "putBits measure reject");
}

} // namespace

int main()
{
    testSpecPatterns();
    testAbcLayout();
    testWidthsAndLimits();
    testEndianSplit();
    testOffsetMatrix();
    testValueAtOffsets();
    testObjectsAndMeasure();
    testBoolRangeAlign();
    testBadRangeAndPad();
    testOverflow();
    testBytesMixAndReuse();
    testSignedEnumFloat();
    testMeasureMixed();
    testZeroWidthAndReject();
    testReadEnd();
    testPutBitsWidth();
    if (g_fail != 0) {
        std::cerr << g_fail << " failed\n";
        return 1;
    }
    std::cout << "OK msb bitpack\n";
    return 0;
}
