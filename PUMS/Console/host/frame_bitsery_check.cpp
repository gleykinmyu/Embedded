/**
 * Заголовки и PDU на одном MSB-first курсоре.
 * Байты заданы заранее. Round-trip их не заменяет.
 *
 * Message: txn, затем type, затем 5 бит CtrlHeader. Это новый порядок CAN ID.
 * Control: op по-прежнему 4 бита сразу после proto.
 */

#include <cstdint>
#include <cstring>
#include <iostream>

#include "smcp/new_smcp/proto/base/header.hpp"
#include "smcp/new_smcp/proto/control/header.hpp"
#include "smcp/new_smcp/proto/control/pdu.hpp"
#include "smcp/new_smcp/proto/msg/header.hpp"
#include "smcp/new_smcp/proto/msg/pdu.hpp"

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

/** Первые nbits потока, старший бит байта 0 — старший бит числа. */
uint32_t msbToInt(const uint8_t* bytes, size_t nbits)
{
    uint32_t value = 0;
    for (size_t i = 0; i < nbits; ++i) {
        const unsigned bit = (bytes[i / 8u] >> (7u - (i % 8u))) & 1u;
        value = (value << 1) | bit;
    }
    return value;
}

struct Written {
    bitsery::custom::msb::WriteError err = bitsery::custom::msb::WriteError::NoError;
    size_t bits = 0;
    size_t bytes = 0;
};

template <typename F>
Written writeWith(uint8_t* buf, size_t cap, F fn)
{
    bitsery::custom::Writer ser{bitsery::custom::msb::OutputAdapter<>{buf, cap}};
    fn(ser);
    return {ser.adapter().error(), ser.adapter().bitPos(), ser.adapter().writtenBytesCount()};
}

template <typename F>
Written measureWith(F fn)
{
    bitsery::custom::Measure ser{bitsery::custom::msb::MeasureAdapter<>{}};
    fn(ser);
    return {ser.adapter().error(), ser.adapter().bitPos(), ser.adapter().writtenBytesCount()};
}

smcp::proto::base::Header baseMsg()
{
    smcp::proto::base::Header base{};
    base.prio = smcp::proto::base::Priority::Max;
    base.dst = smcp::proto::base::addr_t{0x10};
    base.src = smcp::proto::base::addr_t{0x01};
    base.proto = smcp::proto::base::Proto::Message;
    return base;
}

smcp::proto::base::Header baseCtrl()
{
    auto base = baseMsg();
    base.proto = smcp::proto::base::Proto::Control;
    return base;
}

void checkPair(const Written& got, const Written& measured, size_t bits, const char* what)
{
    check(got.err == bitsery::custom::msb::WriteError::NoError, what);
    check(got.bits == bits, what);
    check(measured.err == got.err && measured.bits == got.bits && measured.bytes == got.bytes, what);
}

void testBaseLimits()
{
    smcp::proto::base::Header hi{};
    hi.prio = smcp::proto::base::Priority::Low;
    hi.dst = smcp::proto::base::addr_t{0x7F};
    hi.src = smcp::proto::base::addr_t{0x7F};
    hi.proto = smcp::proto::base::Proto::Message;
    uint8_t buf[4]{};
    const auto got = writeWith(buf, sizeof buf, [&](auto& s) { s.object(hi); });
    const auto measured = measureWith([&](auto& s) { s.object(hi); });
    const uint8_t exp[] = {0xFF, 0xFF, 0x80};
    checkPair(got, measured, 17u, "base max");
    expectBytes(buf, 3, exp, "base max");

    bitsery::custom::Reader des{bitsery::custom::msb::InputAdapter<>{buf, 3}};
    smcp::proto::base::Header back{};
    des.object(back);
    check(back.prio == hi.prio && back.dst == hi.dst && back.src == hi.src && back.proto == hi.proto,
          "base max read");
    check(des.adapter().bitPos() == 17u, "base max bits read");

    smcp::proto::base::Header lo{};
    lo.prio = smcp::proto::base::Priority::Max;
    lo.proto = smcp::proto::base::Proto::Control;
    uint8_t zero[4]{};
    const auto z = writeWith(zero, sizeof zero, [&](auto& s) { s.object(lo); });
    check(z.bits == 17u && z.err == bitsery::custom::msb::WriteError::NoError, "base min");
    const uint8_t zexp[] = {0x00, 0x00, 0x00};
    expectBytes(zero, 3, zexp, "base min");

    smcp::proto::base::Header bad = lo;
    bad.dst.v = 0x80;
    uint8_t dump[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    const auto rejected = writeWith(dump, sizeof dump, [&](auto& s) { s.object(bad); });
    const auto rejectedM = measureWith([&](auto& s) { s.object(bad); });
    check(rejected.err == bitsery::custom::msb::WriteError::InvalidValue && rejected.bits == 2u, "dst 0x80");
    check(rejectedM.err == rejected.err && rejectedM.bits == rejected.bits, "dst measure");

    smcp::proto::base::Header prio = lo;
    prio.prio = smcp::proto::base::Priority{4};
    const auto badPrio = writeWith(dump, sizeof dump, [&](auto& s) { s.object(prio); });
    check(badPrio.err == bitsery::custom::msb::WriteError::InvalidValue && badPrio.bits == 0u, "prio 4");

    uint8_t shortBuf[2]{};
    const auto overflow = writeWith(shortBuf, sizeof shortBuf, [&](auto& s) { s.object(lo); });
    check(overflow.err == bitsery::custom::msb::WriteError::BufferOverflow && overflow.bits != 17u, "base short out");

    bitsery::custom::Reader cut{bitsery::custom::msb::InputAdapter<>{zero, 2}};
    smcp::proto::base::Header cutBack{};
    cut.object(cutBack);
    check(cut.adapter().error() == bitsery::ReaderError::DataOverflow, "base short in");
    check(!cut.adapter().isCompletedSuccessfully(), "base no success");
}

void testControl()
{
    auto base = baseCtrl();
    smcp::proto::control::Header hdr{};
    smcp::proto::control::HeartBeat hb{};
    hb.version = MISC::FwVersion{1, 2, 3};

    uint8_t buf[16]{};
    const auto got = writeWith(buf, sizeof buf, [&](auto& s) {
        s.object(base);
        check(s.adapter().bitPos() == 17u, "ctrl after base");
        s.object(hdr);
        check(s.adapter().bitPos() == 21u, "ctrl after op");
        s.object(hb);
    });
    const auto measured = measureWith([&](auto& s) {
        s.object(base);
        s.object(hdr);
        s.object(hb);
    });
    const uint8_t exp[] = {0x08, 0x01, 0x00, 0x00, 0x08, 0x10, 0x18, 0x00};
    checkPair(got, measured, 61u, "heartbeat");
    expectBytes(buf, 8, exp, "heartbeat");

    bitsery::custom::Reader des{bitsery::custom::msb::InputAdapter<>{buf, 8}};
    smcp::proto::base::Header base2{};
    smcp::proto::control::Header hdr2{};
    smcp::proto::control::HeartBeat hb2{};
    des.object(base2);
    des.object(hdr2);
    des.object(hb2);
    check(base2.proto == smcp::proto::base::Proto::Control, "hb proto");
    check(hdr2.op == smcp::proto::control::Type::HeartBeat, "hb op");
    check(hb2.msg == smcp::proto::control::HeartBeat::Msg::Request, "hb msg");
    check(hb2.version.major == 1 && hb2.version.minor == 2 && hb2.version.patch == 3, "hb ver");
    check(des.adapter().bitPos() == 61u, "hb read bits");

    smcp::proto::control::HeartBeat busy{};
    busy.msg = smcp::proto::control::HeartBeat::Msg::Busy;
    uint8_t bbuf[8]{};
    const auto bgot = writeWith(bbuf, sizeof bbuf, [&](auto& s) {
        s.object(base);
        s.object(hdr);
        s.object(busy);
    });
    const uint8_t bexp[] = {0x08, 0x01, 0x00, 0x10};
    check(bgot.bits == 29u && bgot.err == bitsery::custom::msb::WriteError::NoError, "busy");
    expectBytes(bbuf, 4, bexp, "busy");
    check(measureWith([&](auto& s) {
              s.object(base);
              s.object(hdr);
              s.object(busy);
          }).bits == 29u,
          "busy measure");

    smcp::proto::control::Header faultHdr{};
    faultHdr.op = smcp::proto::control::Type::Fault;
    smcp::proto::control::Fault fault{};
    fault.detail = 0xABCD;
    std::memcpy(fault.name, "FAULT01", smcp::proto::control::Fault::kNameLen);
    uint8_t fbuf[16]{};
    const auto fgot = writeWith(fbuf, sizeof fbuf, [&](auto& s) {
        s.object(base);
        s.object(faultHdr);
        s.object(fault);
    });
    const uint8_t fexp[] = {0x08, 0x01, 0x0E, 0x6D, 0x5A, 0x32, 0x0A, 0xAA, 0x62, 0xA1, 0x81, 0x88};
    checkPair(fgot, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(faultHdr);
                  s.object(fault);
              }),
              93u, "fault");
    expectBytes(fbuf, 12, fexp, "fault");

    bitsery::custom::Reader fd{bitsery::custom::msb::InputAdapter<>{fbuf, 12}};
    smcp::proto::base::Header fb{};
    smcp::proto::control::Header fh{};
    smcp::proto::control::Fault ff{};
    fd.object(fb);
    fd.object(fh);
    fd.object(ff);
    check(fh.op == smcp::proto::control::Type::Fault && ff.detail == 0xABCD, "fault read");
    check(std::memcmp(ff.name, "FAULT01", 7) == 0, "fault name");

    smcp::proto::control::Header arpHdr{};
    arpHdr.op = smcp::proto::control::Type::Arp;
    smcp::proto::control::Arp arp{};
    arp.op = smcp::proto::control::Arp::Op::Rep;
    std::memcpy(arp.name, "NODE0001", smcp::proto::control::Arp::kNameLen);
    uint8_t abuf[16]{};
    const auto agot = writeWith(abuf, sizeof abuf, [&](auto& s) {
        s.object(base);
        s.object(arpHdr);
        s.object(arp);
    });
    const uint8_t aexp[] = {0x08, 0x01, 0x10, 0x0A, 0x72, 0x7A, 0x22, 0x29, 0x81, 0x81, 0x81, 0x88};
    checkPair(agot, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(arpHdr);
                  s.object(arp);
              }),
              93u, "arp");
    expectBytes(abuf, 12, aexp, "arp");

    bitsery::custom::Reader ad{bitsery::custom::msb::InputAdapter<>{abuf, 12}};
    smcp::proto::control::Header ah{};
    smcp::proto::control::Arp aa{};
    smcp::proto::base::Header ab{};
    ad.object(ab);
    ad.object(ah);
    ad.object(aa);
    check(ah.op == smcp::proto::control::Type::Arp && aa.op == smcp::proto::control::Arp::Op::Rep, "arp read");
    check(std::memcmp(aa.name, "NODE0001", 8) == 0, "arp name");

    smcp::proto::control::Header wide{};
    wide.op = smcp::proto::control::Type{0x10};
    const auto bad = writeWith(buf, sizeof buf, [&](auto& s) {
        s.object(base);
        s.object(wide);
    });
    check(bad.err == bitsery::custom::msb::WriteError::InvalidValue && bad.bits == 17u, "op 0x10");
}

void testMessage()
{
    auto base = baseMsg();
    smcp::proto::msg::Header msg{};
    msg.txn = 0x15;
    smcp::proto::msg::CtrlHeader id{};

    uint8_t buf[16]{};
    const auto got = writeWith(buf, sizeof buf, [&](auto& s) {
        s.object(base);
        check(s.adapter().bitPos() == 17u, "msg after base");
        s.object(msg);
        check(s.adapter().bitPos() == 24u, "msg after header");
        s.object(id);
        check(s.adapter().bitPos() == 29u, "msg after id");
        smcp::proto::msg::Ack ack{};
        s.object(ack);
    });
    const uint8_t exp[] = {0x08, 0x01, 0xAA, 0x00};
    checkPair(got, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(msg);
                  s.object(id);
                  smcp::proto::msg::Ack ack{};
                  s.object(ack);
              }),
              29u, "ack");
    expectBytes(buf, 4, exp, "ack");
    check(msbToInt(buf, 29) == 0x01003540u, "ack can id");

    bitsery::custom::Reader des{bitsery::custom::msb::InputAdapter<>{buf, 4}};
    des.adapter().currentReadEndPos(4);
    smcp::proto::base::Header b2{};
    smcp::proto::msg::Header m2{};
    smcp::proto::msg::CtrlHeader i2{};
    smcp::proto::msg::Ack a2{};
    des.object(b2);
    des.object(m2);
    des.object(i2);
    des.object(a2);
    check(m2.txn == 0x15 && m2.type == smcp::proto::msg::Type::CtrlMsg, "ack header");
    check(i2.id == smcp::proto::msg::CtrlId::Ack, "ack id");
    check(des.adapter().bitPos() == 29u, "ack read bits");
    check(!des.adapter().isCompletedSuccessfully(), "ack pad left");
    des.adapter().align();
    check(des.adapter().error() == bitsery::ReaderError::NoError, "ack pad");
    check(des.adapter().isCompletedSuccessfully(), "ack complete");

    smcp::proto::msg::CtrlHeader nackId{};
    nackId.id = smcp::proto::msg::CtrlId::Nack;
    smcp::proto::msg::Nack nack{};
    nack.error = smcp::proto::msg::Nack::Error::Timeout;
    uint8_t nbuf[8]{};
    const auto ngot = writeWith(nbuf, sizeof nbuf, [&](auto& s) {
        s.object(base);
        s.object(msg);
        s.object(nackId);
        s.object(nack);
    });
    const uint8_t nexp[] = {0x08, 0x01, 0xAA, 0x08, 0x18};
    checkPair(ngot, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(msg);
                  s.object(nackId);
                  s.object(nack);
              }),
              37u, "nack");
    expectBytes(nbuf, 5, nexp, "nack");

    bitsery::custom::Reader nd{bitsery::custom::msb::InputAdapter<>{nbuf, 5}};
    smcp::proto::msg::CtrlHeader ni{};
    smcp::proto::msg::Nack nn{};
    smcp::proto::base::Header nb{};
    smcp::proto::msg::Header nm{};
    nd.object(nb);
    nd.object(nm);
    nd.object(ni);
    nd.object(nn);
    check(ni.id == smcp::proto::msg::CtrlId::Nack && nn.error == smcp::proto::msg::Nack::Error::Timeout,
          "nack read");

    smcp::proto::msg::CtrlHeader segId{};
    segId.id = smcp::proto::msg::CtrlId::SegData;
    smcp::proto::msg::seg::SegHeader seg{};
    smcp::proto::msg::seg::SegBegin begin{};
    begin.count = 4;
    uint8_t sbuf[16]{};
    const auto sgot = writeWith(sbuf, sizeof sbuf, [&](auto& s) {
        s.object(base);
        check(s.adapter().bitPos() == 17u, "seg after base");
        s.object(msg);
        check(s.adapter().bitPos() == 24u, "seg after msg");
        s.object(segId);
        check(s.adapter().bitPos() == 29u, "seg after id");
        s.object(seg);
        s.object(begin);
    });
    const uint8_t sexp[] = {0x08, 0x01, 0xAA, 0x10, 0x00, 0x00, 0x20, 0x00};
    checkPair(sgot, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(msg);
                  s.object(segId);
                  s.object(seg);
                  s.object(begin);
              }),
              61u, "begin");
    expectBytes(sbuf, 8, sexp, "begin");
    check((sgot.bits - smcp::proto::CanLimits::kIdBits) / 8u == 4u, "begin can data");
    check(seg.isBegin(), "begin flag");

    bitsery::custom::Reader sd{bitsery::custom::msb::InputAdapter<>{sbuf, 8}};
    smcp::proto::msg::seg::SegHeader seg2{};
    smcp::proto::msg::seg::SegBegin begin2{};
    smcp::proto::base::Header sb{};
    smcp::proto::msg::Header sm{};
    smcp::proto::msg::CtrlHeader si{};
    sd.object(sb);
    sd.object(sm);
    sd.object(si);
    sd.object(seg2);
    sd.object(begin2);
    check(si.id == smcp::proto::msg::CtrlId::SegData && seg2.isBegin() && begin2.count == 4, "begin read");

    smcp::proto::msg::seg::SegHeader dataSeg{};
    dataSeg.seg = 1;
    smcp::proto::msg::seg::SegData payload{};
    const uint8_t rawData[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    std::memcpy(payload.data, rawData, sizeof rawData);
    uint8_t dbuf[16]{};
    const auto dgot = writeWith(dbuf, sizeof dbuf, [&](auto& s) {
        s.object(base);
        s.object(msg);
        s.object(segId);
        s.object(dataSeg);
        s.object(payload);
    });
    const uint8_t dexp[] = {0x08, 0x01, 0xAA, 0x10, 0x08, 0x00, 0x89, 0x11, 0x9A, 0x22, 0xAB, 0x30};
    checkPair(dgot, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(msg);
                  s.object(segId);
                  s.object(dataSeg);
                  s.object(payload);
              }),
              93u, "seg data");
    expectBytes(dbuf, 12, dexp, "seg data");
    check((dgot.bits - smcp::proto::CanLimits::kIdBits) / 8u == 8u, "data can bytes");

    uint8_t alone[8]{};
    const auto aloneGot = writeWith(alone, sizeof alone, [&](auto& s) { s.object(payload); });
    check(aloneGot.bits == smcp::proto::msg::seg::SegData::kBits && aloneGot.bytes == 6u, "no size prefix");
    expectBytes(alone, 6, rawData, "data bytes");

    bitsery::custom::Reader dd{bitsery::custom::msb::InputAdapter<>{dbuf, 12}};
    smcp::proto::msg::seg::SegHeader dataSeg2{};
    smcp::proto::msg::seg::SegData payload2{};
    smcp::proto::base::Header db{};
    smcp::proto::msg::Header dm{};
    smcp::proto::msg::CtrlHeader di{};
    dd.object(db);
    dd.object(dm);
    dd.object(di);
    dd.object(dataSeg2);
    dd.object(payload2);
    check(dataSeg2.seg == 1 && std::memcmp(payload2.data, rawData, 6) == 0, "data read");

    seg.seg = smcp::proto::msg::seg::SegHeader::kEnd;
    smcp::proto::msg::seg::SegEnd end{};
    end.len = 6;
    end.crc = 0xA1B2C3D4u;
    uint8_t ebuf[16]{};
    const auto egot = writeWith(ebuf, sizeof ebuf, [&](auto& s) {
        s.object(base);
        s.object(msg);
        s.object(segId);
        s.object(seg);
        s.object(end);
    });
    const uint8_t eexp[] = {0x08, 0x01, 0xAA, 0x17, 0xFF, 0xF8, 0x36, 0xA6, 0x1D, 0x95, 0x08};
    checkPair(egot, measureWith([&](auto& s) {
                  s.object(base);
                  s.object(msg);
                  s.object(segId);
                  s.object(seg);
                  s.object(end);
              }),
              85u, "end");
    expectBytes(ebuf, 11, eexp, "end");
    check((egot.bits - smcp::proto::CanLimits::kIdBits) / 8u == 7u, "end can data");
    check(seg.isEnd(), "end flag");

    bitsery::custom::Reader ed{bitsery::custom::msb::InputAdapter<>{ebuf, 11}};
    smcp::proto::msg::seg::SegHeader seg3{};
    smcp::proto::msg::seg::SegEnd end2{};
    smcp::proto::base::Header eb{};
    smcp::proto::msg::Header em{};
    smcp::proto::msg::CtrlHeader ei{};
    ed.object(eb);
    ed.object(em);
    ed.object(ei);
    ed.object(seg3);
    ed.object(end2);
    check(seg3.isEnd() && end2.len == 6 && end2.crc == 0xA1B2C3D4u, "end read");

    smcp::proto::msg::Header top{};
    top.txn = smcp::proto::msg::Header::kTxnMax;
    top.type = smcp::proto::msg::Type::AppMsg;
    smcp::proto::msg::CtrlHeader full{};
    full.id = smcp::proto::msg::CtrlId{0x1F};
    uint8_t tbuf[4]{};
    const auto tgot = writeWith(tbuf, sizeof tbuf, [&](auto& s) {
        s.object(base);
        s.object(top);
        s.object(full);
    });
    const uint8_t texp[] = {0x08, 0x01, 0xFF, 0xF8};
    check(tgot.bits == 29u && tgot.err == bitsery::custom::msb::WriteError::NoError, "msg max");
    expectBytes(tbuf, 4, texp, "msg max");
    check(msbToInt(tbuf, 29) == 0x01003FFFu, "msg max can id");

    smcp::proto::msg::Header badType{};
    badType.type = smcp::proto::msg::Type{2};
    const auto bad = writeWith(buf, sizeof buf, [&](auto& s) {
        s.object(base);
        s.object(badType);
    });
    check(bad.err == bitsery::custom::msb::WriteError::InvalidValue && bad.bits == 23u, "type 2");

    uint8_t tiny[3]{};
    const auto overflow = writeWith(tiny, sizeof tiny, [&](auto& s) {
        s.object(base);
        s.object(msg);
        s.object(id);
    });
    check(overflow.err == bitsery::custom::msb::WriteError::BufferOverflow, "msg short out");
    check(overflow.bits != 29u, "msg not success");
}

void testSegmentLengths()
{
    auto bitsOf = [](auto& obj) {
        return measureWith([&](auto& s) { s.object(obj); }).bits;
    };
    smcp::proto::base::Header base{};
    smcp::proto::control::Header ctrl{};
    smcp::proto::control::HeartBeat hb{};
    smcp::proto::control::HeartBeat busy{};
    busy.msg = smcp::proto::control::HeartBeat::Msg::Busy;
    smcp::proto::control::Fault fault{};
    smcp::proto::control::Arp arp{};
    smcp::proto::msg::Header msg{};
    smcp::proto::msg::CtrlHeader id{};
    smcp::proto::msg::Ack ack{};
    smcp::proto::msg::Nack nack{};
    smcp::proto::msg::seg::SegHeader seg{};
    smcp::proto::msg::seg::SegBegin begin{};
    smcp::proto::msg::seg::SegData data{};
    smcp::proto::msg::seg::SegEnd end{};
    check(bitsOf(base) == smcp::proto::base::Header::kBits, "base bits");
    check(bitsOf(ctrl) == smcp::proto::control::Header::kBits, "ctrl bits");
    check(bitsOf(hb) == smcp::proto::control::HeartBeat::kBits, "hb bits");
    check(bitsOf(busy) == smcp::proto::control::HeartBeat::kBusyBits, "busy bits");
    check(bitsOf(fault) == smcp::proto::control::Fault::kBits, "fault bits");
    check(bitsOf(arp) == smcp::proto::control::Arp::kBits, "arp bits");
    check(bitsOf(msg) == smcp::proto::msg::Header::kBits, "msg bits");
    check(bitsOf(id) == smcp::proto::msg::CtrlHeader::kBits, "id bits");
    check(bitsOf(ack) == smcp::proto::msg::Ack::kBits, "ack bits");
    check(bitsOf(nack) == smcp::proto::msg::Nack::kBits, "nack bits");
    check(bitsOf(seg) == smcp::proto::msg::seg::SegHeader::kBits, "seg bits");
    check(bitsOf(begin) == smcp::proto::msg::seg::SegBegin::kBits, "begin bits");
    check(bitsOf(data) == smcp::proto::msg::seg::SegData::kBits, "data bits");
    check(bitsOf(end) == smcp::proto::msg::seg::SegEnd::kBits, "end bits");

    using smcp::proto::msg::seg::dataCount;
    using smcp::proto::msg::seg::kMessageSizeMax;
    using smcp::proto::msg::seg::kSegCountMax;
    using smcp::proto::msg::seg::kSegDataMax;
    using smcp::proto::msg::seg::lastLen;
    using smcp::proto::msg::seg::messageSize;
    using smcp::proto::msg::seg::padBytes;

    check(dataCount(0) == 0 && lastLen(0) == 0 && padBytes(0) == 0, "empty");
    check(messageSize(0, 0) == 0, "empty restore");
    check(dataCount(1) == 1 && lastLen(1) == 1 && padBytes(1) == 5, "one byte");
    check(dataCount(6) == 1 && lastLen(6) == 6 && padBytes(6) == 0, "one full");
    check(dataCount(7) == 2 && lastLen(7) == 1 && padBytes(7) == 5, "two segs");
    check(dataCount(kMessageSizeMax) == kSegCountMax, "max count");
    check(lastLen(kMessageSizeMax) == kSegDataMax && padBytes(kMessageSizeMax) == 0, "max full");

    bool all = true;
    for (size_t n = 0; n <= kMessageSizeMax; ++n) {
        const size_t count = dataCount(n);
        const size_t last = lastLen(n);
        const size_t pad = padBytes(n);
        if (count > kSegCountMax) {
            all = false;
            break;
        }
        if (count == 0) {
            all = n == 0 && last == 0 && pad == 0 && messageSize(0, 0) == 0;
        } else {
            all = last >= 1 && last <= kSegDataMax && pad == kSegDataMax - last
                && count <= 0xFFFEu && messageSize(count, last) == n;
        }
        if (!all) {
            break;
        }
    }
    check(all, "every length");
}

} // namespace

int main()
{
    testBaseLimits();
    testControl();
    testMessage();
    testSegmentLengths();
    if (g_fail != 0) {
        std::cerr << g_fail << " failed\n";
        return 1;
    }
    std::cout << "OK headers\n";
    return 0;
}
