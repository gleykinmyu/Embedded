#include "nexCommands.hpp"
#include "../comp/nexLiterals.hpp"
#include <cstdint>
#include <cstring>

namespace {

bool printUint32Digits(nex::TxFrame& tx, uint32_t value) noexcept {
    uint8_t digits[10];
    uint8_t n = 0;
    if (value == 0u) {
        digits[0] = static_cast<uint8_t>('0');
        n = 1u;
    } else {
        uint8_t rev[10];
        uint8_t r = 0;
        while (value > 0u && r < sizeof(rev)) {
            rev[r++] = static_cast<uint8_t>('0' + (value % 10u));
            value /= 10u;
        }
        for (uint8_t i = 0; i < r; ++i)
            digits[i] = rev[r - 1u - i];
        n = r;
    }
    return tx.pushBytes(digits, n);
}

static const char* numericOpAscii(nex::cmd::assign::Numeric::Op op) noexcept {
    using Op = nex::cmd::assign::Numeric::Op;
    switch (op) {
    case Op::Assign:
        return "=";
    case Op::Add:
        return "+=";
    case Op::Sub:
        return "-=";
    case Op::Mul:
        return "*=";
    case Op::Div:
        return "/=";
    case Op::Mod:
        return "%=";
    }
    return "=";
}

static uint8_t numericOpLen(nex::cmd::assign::Numeric::Op op) noexcept {
    using Op = nex::cmd::assign::Numeric::Op;
    switch (op) {
    case Op::Assign:
        return 1u;
    case Op::Add:
    case Op::Sub:
    case Op::Mul:
    case Op::Div:
    case Op::Mod:
        return 2u;
    }
    return 1u;
}

static bool pushLit(nex::TxFrame& tx, const char* s) noexcept {
    if (s == nullptr)
        return false;
    const size_t n = std::strlen(s);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(s, static_cast<uint16_t>(n));
}

static bool pushSp(nex::TxFrame& tx) noexcept {
    const uint8_t sp = static_cast<uint8_t>(' ');
    return tx.pushBytes(&sp, 1u);
}

static bool pushDot(nex::TxFrame& tx) noexcept {
    const uint8_t dot = static_cast<uint8_t>('.');
    return tx.pushBytes(&dot, 1u);
}

} // namespace

namespace nex {
namespace cmd {

bool appendTargetLexeme(TxFrame& tx, const TargetAttr& t) noexcept {
    if (t.attr == nullptr)
        return false;
    const TargetComponent& c = t.comp;
    if (c.page == nullptr && c.name == nullptr)
        return pushLit(tx, t.attr);
    if (c.page == nullptr && c.name != nullptr) {
        const size_t nn = std::strlen(c.name);
        const size_t na = std::strlen(t.attr);
        if (nn == 0u || na == 0u || nn > static_cast<size_t>(UINT16_MAX) || na > static_cast<size_t>(UINT16_MAX))
            return false;
        if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(nn) + 1u + static_cast<uint32_t>(na) > TxFrame::MAX_PAYLOAD)
            return false;
        return tx.pushBytes(c.name, static_cast<uint16_t>(nn)) && pushDot(tx) &&
               tx.pushBytes(t.attr, static_cast<uint16_t>(na));
    }
    if (c.page != nullptr && c.name != nullptr) {
        const size_t np = std::strlen(c.page);
        const size_t nn = std::strlen(c.name);
        const size_t na = std::strlen(t.attr);
        if (np == 0u || nn == 0u || na == 0u || np > static_cast<size_t>(UINT16_MAX) || nn > static_cast<size_t>(UINT16_MAX) ||
            na > static_cast<size_t>(UINT16_MAX))
            return false;
        if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(np) + 1u + static_cast<uint32_t>(nn) + 1u +
                static_cast<uint32_t>(na) >
            TxFrame::MAX_PAYLOAD)
            return false;
        return tx.pushBytes(c.page, static_cast<uint16_t>(np)) && pushDot(tx) &&
               tx.pushBytes(c.name, static_cast<uint16_t>(nn)) && pushDot(tx) &&
               tx.pushBytes(t.attr, static_cast<uint16_t>(na));
    }
    return false;
}

bool appendTargetComponentLexeme(TxFrame& tx, const TargetComponent& c) noexcept {
    if (c.page == nullptr && c.name == nullptr)
        return false;
    if (c.page == nullptr && c.name != nullptr) {
        const size_t nn = std::strlen(c.name);
        if (nn == 0u || nn > static_cast<size_t>(UINT16_MAX))
            return false;
        if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(nn) > TxFrame::MAX_PAYLOAD)
            return false;
        return tx.pushBytes(c.name, static_cast<uint16_t>(nn));
    }
    if (c.page != nullptr && c.name != nullptr) {
        const size_t np = std::strlen(c.page);
        const size_t nn = std::strlen(c.name);
        if (np == 0u || nn == 0u || np > static_cast<size_t>(UINT16_MAX) || nn > static_cast<size_t>(UINT16_MAX))
            return false;
        if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(np) + 1u + static_cast<uint32_t>(nn) > TxFrame::MAX_PAYLOAD)
            return false;
        return tx.pushBytes(c.page, static_cast<uint16_t>(np)) && pushDot(tx) &&
               tx.pushBytes(c.name, static_cast<uint16_t>(nn));
    }
    return false;
}

} // namespace cmd

bool Command::appendComma(TxFrame& tx) noexcept {
    const uint8_t comma = static_cast<uint8_t>(',');
    return tx.pushBytes(&comma, 1u);
}

bool Command::appendUint32(TxFrame& tx, uint32_t value) noexcept {
    return printUint32Digits(tx, value);
}

bool Command::appendInt32(TxFrame& tx, int32_t value) noexcept {
    if (value >= 0)
        return printUint32Digits(tx, static_cast<uint32_t>(value));
    const uint8_t minus = static_cast<uint8_t>('-');
    if (!tx.pushBytes(&minus, 1u))
        return false;
    uint32_t u = static_cast<uint32_t>(value);
    u = ~u + 1u;
    return printUint32Digits(tx, u);
}

bool cmd::assign::Text::serialize(TxFrame& tx) const noexcept {
    (void)tx;
    (void)_target;
    (void)text;
    return false;
}

bool cmd::assign::TextAppend::serialize(TxFrame& tx) const noexcept {
    (void)tx;
    (void)_target;
    (void)text;
    return false;
}

bool cmd::assign::TextSubtract::serialize(TxFrame& tx) const noexcept {
    (void)tx;
    (void)_target;
    (void)_charsCount;
    return false;
}

bool cmd::assign::Numeric::serialize(TxFrame& tx) const noexcept {
    const uint8_t opLen = numericOpLen(_op);
    if (!cmd::appendTargetLexeme(tx, _target))
        return false;
    if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(opLen) + 11u > TxFrame::MAX_PAYLOAD)
        return false;
    const char* opStr = numericOpAscii(_op);
    if (!tx.pushBytes(opStr, opLen))
        return false;
    return appendInt32(tx, _value);
}

// --- NIS §3 operational (nex::cmd::oper) ---

bool cmd::oper::Page::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::page_open) || !pushSp(tx))
        return false;
    return appendUint32(tx, static_cast<uint32_t>(_pageID));
}

bool cmd::oper::Refresh::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::refresh) || !pushSp(tx))
        return false;
    return cmd::appendTargetComponentLexeme(tx, _component);
}

bool cmd::oper::RefreshStop::serialize(TxFrame& tx) const noexcept {
    return pushLit(tx, "ref_stop");
}

bool cmd::oper::RefreshStart::serialize(TxFrame& tx) const noexcept {
    return pushLit(tx, "ref_star");
}

bool cmd::oper::Get::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::get_attr) || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _operand);
}

bool cmd::oper::SendMe::serialize(TxFrame& tx) const noexcept {
    return pushLit(tx, nex::cmd::send_page_id);
}

bool cmd::oper::Convert::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "cov") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _src) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dst) && appendComma(tx) &&
           appendInt32(tx, _length);
}

bool cmd::oper::ConvertEx::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "covx") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _src) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dst) && appendComma(tx) &&
           appendInt32(tx, _length) && appendComma(tx) && appendInt32(tx, _format);
}

bool cmd::oper::Substr::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "substr") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _fromTxt) && appendComma(tx) && cmd::appendTargetLexeme(tx, _toTxt) && appendComma(tx) &&
           appendUint32(tx, _start) && appendComma(tx) && appendUint32(tx, _count);
}

bool cmd::oper::Strlen::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "strlen") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _txtAttr) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dstNumAttr);
}

bool cmd::oper::Btlen::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "btlen") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _txtAttr) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dstNumAttr);
}

bool cmd::oper::Spstr::serialize(TxFrame& tx) const noexcept {
    if (_delimiterQuoted == nullptr)
        return false;
    if (!pushLit(tx, "spstr") || !pushSp(tx))
        return false;
    const size_t ndq = std::strlen(_delimiterQuoted);
    if (ndq == 0u || ndq > static_cast<size_t>(UINT16_MAX))
        return false;
    return cmd::appendTargetLexeme(tx, _src) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dst) && appendComma(tx) &&
           tx.pushBytes(_delimiterQuoted, static_cast<uint16_t>(ndq)) && appendComma(tx) && appendUint32(tx, _index);
}

bool cmd::oper::TouchJ::serialize(TxFrame& tx) const noexcept {
    return pushLit(tx, "touch_j");
}

bool cmd::oper::Vis::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::visibility) || !pushSp(tx))
        return false;
    return cmd::appendTargetComponentLexeme(tx, _component) && appendComma(tx) && appendUint32(tx, static_cast<uint32_t>(_state));
}

bool cmd::oper::Tsw::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::touch_switch) || !pushSp(tx))
        return false;
    return cmd::appendTargetComponentLexeme(tx, _component) && appendComma(tx) &&
           appendUint32(tx, static_cast<uint32_t>(_enabled));
}

bool cmd::oper::Click::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::click_sim) || !pushSp(tx))
        return false;
    return cmd::appendTargetComponentLexeme(tx, _component) && appendComma(tx) &&
           appendUint32(tx, static_cast<uint32_t>(_press1Release0));
}

bool cmd::oper::Randset::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "randset") || !pushSp(tx))
        return false;
    return appendInt32(tx, _min) && appendComma(tx) && appendInt32(tx, _max);
}

bool cmd::oper::WaveAdd::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "add") || !pushSp(tx))
        return false;
    return appendUint32(tx, _waveformId) && appendComma(tx) && appendUint32(tx, _channel) && appendComma(tx) &&
           appendUint32(tx, _value0to255);
}

bool cmd::oper::WaveAddt::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, nex::cmd::wf_add) || !pushSp(tx))
        return false;
    return appendUint32(tx, _waveformId) && appendComma(tx) && appendUint32(tx, _channel) && appendComma(tx) &&
           appendUint32(tx, _byteCount);
}

bool cmd::oper::WaveCle::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "cle") || !pushSp(tx))
        return false;
    return appendUint32(tx, _waveformId) && appendComma(tx) && appendUint32(tx, _channelOr255All);
}

bool cmd::oper::Rest::serialize(TxFrame& tx) const noexcept {
    return pushLit(tx, "rest");
}

bool cmd::oper::DoEvents::serialize(TxFrame& tx) const noexcept {
    return pushLit(tx, nex::cmd::do_events);
}

bool cmd::oper::Wepo::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "wepo") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _source) && appendComma(tx) && appendUint32(tx, _addr);
}

bool cmd::oper::Repo::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "repo") || !pushSp(tx))
        return false;
    return cmd::appendTargetLexeme(tx, _dest) && appendComma(tx) && appendUint32(tx, _addr);
}

bool cmd::oper::Wept::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "wept") || !pushSp(tx))
        return false;
    return appendUint32(tx, _addr) && appendComma(tx) && appendUint32(tx, _byteCount);
}

bool cmd::oper::Rept::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "rept") || !pushSp(tx))
        return false;
    return appendUint32(tx, _addr) && appendComma(tx) && appendUint32(tx, _byteCount);
}

bool cmd::oper::Cfgpio::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "cfgpio") || !pushSp(tx))
        return false;
    return appendUint32(tx, _pin) && appendComma(tx) && appendUint32(tx, _mode) && appendComma(tx) &&
           cmd::appendTargetComponentLexeme(tx, _bindComponentOrZero);
}

bool cmd::oper::Move::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "move") || !pushSp(tx))
        return false;
    return cmd::appendTargetComponentLexeme(tx, _component) && appendComma(tx) && appendInt32(tx, _x0) && appendComma(tx) &&
           appendInt32(tx, _y0) && appendComma(tx) && appendInt32(tx, _x1) && appendComma(tx) && appendInt32(tx, _y1) &&
           appendComma(tx) && appendUint32(tx, _priority) && appendComma(tx) && appendUint32(tx, _timeMs);
}

bool cmd::oper::Play::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "play") || !pushSp(tx))
        return false;
    return appendUint32(tx, _channel) && appendComma(tx) && appendUint32(tx, _resourceId) && appendComma(tx) &&
           appendUint32(tx, _loop01);
}

bool cmd::oper::Twfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "twfile") || !pushSp(tx))
        return false;
    const size_t np = std::strlen(_pathQuoted);
    if (np == 0u || np > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(np)) && appendComma(tx) && appendUint32(tx, _fileSize);
}

bool cmd::oper::Delfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "delfile") || !pushSp(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool cmd::oper::Refile::serialize(TxFrame& tx) const noexcept {
    if (_pathFromQuoted == nullptr || _pathToQuoted == nullptr)
        return false;
    if (!pushLit(tx, "refile") || !pushSp(tx))
        return false;
    const size_t na = std::strlen(_pathFromQuoted);
    const size_t nb = std::strlen(_pathToQuoted);
    if (na == 0u || nb == 0u || na > UINT16_MAX || nb > UINT16_MAX)
        return false;
    return tx.pushBytes(_pathFromQuoted, static_cast<uint16_t>(na)) && appendComma(tx) &&
           tx.pushBytes(_pathToQuoted, static_cast<uint16_t>(nb));
}

bool cmd::oper::Findfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "findfile") || !pushSp(tx))
        return false;
    const size_t na = std::strlen(_pathQuoted);
    if (na == 0u || na > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(na)) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dstNumAttr);
}

bool cmd::oper::Rdfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "rdfile") || !pushSp(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n)) && appendComma(tx) && appendUint32(tx, _offset) &&
           appendComma(tx) && appendUint32(tx, _count) && appendComma(tx) && appendUint32(tx, _crcOption);
}

bool cmd::oper::Setlayer::serialize(TxFrame& tx) const noexcept {
    if (!pushLit(tx, "setlayer") || !pushSp(tx))
        return false;
    return cmd::appendTargetComponentLexeme(tx, _component) && appendComma(tx) &&
           cmd::appendTargetComponentLexeme(tx, _aboveOr255);
}

bool cmd::oper::Newdir::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "newdir") || !pushSp(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool cmd::oper::Deldir::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "deldir") || !pushSp(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool cmd::oper::Redir::serialize(TxFrame& tx) const noexcept {
    if (_pathFromQuoted == nullptr || _pathToQuoted == nullptr)
        return false;
    if (!pushLit(tx, "redir") || !pushSp(tx))
        return false;
    const size_t na = std::strlen(_pathFromQuoted);
    const size_t nb = std::strlen(_pathToQuoted);
    if (na == 0u || nb == 0u || na > UINT16_MAX || nb > UINT16_MAX)
        return false;
    return tx.pushBytes(_pathFromQuoted, static_cast<uint16_t>(na)) && appendComma(tx) &&
           tx.pushBytes(_pathToQuoted, static_cast<uint16_t>(nb));
}

bool cmd::oper::Finddir::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "finddir") || !pushSp(tx))
        return false;
    const size_t na = std::strlen(_pathQuoted);
    if (na == 0u || na > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(na)) && appendComma(tx) && cmd::appendTargetLexeme(tx, _dstNumAttr);
}

bool cmd::oper::Newfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!pushLit(tx, "newfile") || !pushSp(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n)) && appendComma(tx) && appendUint32(tx, _reservedSize);
}

} // namespace nex
