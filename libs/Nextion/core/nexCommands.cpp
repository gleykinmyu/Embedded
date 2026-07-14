#include "nexCommands.hpp"
#include "nexDebug.hpp"
#include "nexMessages.hpp"
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <variant>

/** Литерал имени команды NIS в кадр; длина — `sizeof(lit) - 1` (без strlen). `lit` — только строковый литерал `"…"`. */
#define NEX_CMD_PRINT_LIT(tx, lit) pushBytes((tx), (lit), static_cast<uint16_t>(sizeof(lit) - 1u))

namespace nex {

bool Command::pushBytes(TxFrame& tx, const void* src, uint16_t n) const noexcept {
    if (n == 0u)
        return true;
    if (src == nullptr)
        return fail(Status::NullPointer);
    if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(n) > static_cast<uint32_t>(TxFrame::MAX_PAYLOAD))
        return fail(Status::TxFrameOverflow);
    std::memcpy(tx.payload + tx.length, src, n);
    tx.length = static_cast<uint16_t>(tx.length + n);
    return true;
}

bool Command::printLiteral(TxFrame& tx, const Literal& lit) const noexcept {
    if (lit.data == nullptr)
        return fail(Status::NullPointer);
    if (lit.len == 0u)
        return fail(Status::EmptyLiteral);
    return pushBytes(tx, lit.data, lit.len);
}

bool Command::printNisToken(TxFrame& tx, const char* token) const noexcept {
    if (token == nullptr)
        return fail(Status::NullPointer);
    const size_t n = std::strlen(token);
    if (n == 0u)
        return fail(Status::EmptyLiteral);
    if (n > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return pushBytes(tx, token, static_cast<uint16_t>(n));
}

bool Command::printSpace(TxFrame& tx) const noexcept {
    const uint8_t sp = static_cast<uint8_t>(' ');
    return pushBytes(tx, &sp, 1u);
}

bool Command::printDot(TxFrame& tx) const noexcept {
    const uint8_t dot = static_cast<uint8_t>('.');
    return pushBytes(tx, &dot, 1u);
}

bool Command::printComma(TxFrame& tx) const noexcept {
    const uint8_t comma = static_cast<uint8_t>(',');
    return pushBytes(tx, &comma, 1u);
}

bool Command::printQuotedString(TxFrame& tx, const char* text) const noexcept {
    const uint8_t dq = static_cast<uint8_t>('"');
    if (!pushBytes(tx, &dq, 1u))
        return false;
    if (text == nullptr)
        return pushBytes(tx, &dq, 1u);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0u; ++p) {
        const unsigned char c = *p;
        if (c == static_cast<unsigned char>('"')) {
            static const char esc[] = "\\\"";
            if (!pushBytes(tx, esc, 2u))
                return false;
        } else if (c == static_cast<unsigned char>('\\')) {
            static const char esc[] = "\\\\";
            if (!pushBytes(tx, esc, 2u))
                return false;
        } else if (c == static_cast<unsigned char>('\r')) {
            static const char esc[] = "\\r";
            if (!pushBytes(tx, esc, 2u))
                return false;
        } else if (!pushBytes(tx, &c, 1u)) {
            return false;
        }
    }
    return pushBytes(tx, &dq, 1u);
}

bool Command::printUint32(TxFrame& tx, uint32_t value) const noexcept {
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
    return pushBytes(tx, digits, n);
}

bool Command::printInt32(TxFrame& tx, int32_t value) const noexcept {
    if (value >= 0)
        return printUint32(tx, static_cast<uint32_t>(value));
    const uint8_t minus = static_cast<uint8_t>('-');
    if (!pushBytes(tx, &minus, 1u))
        return false;
    uint32_t u = static_cast<uint32_t>(value);
    u = ~u + 1u;
    return printUint32(tx, u);
}

bool Command::printOperation(TxFrame& tx, Op op) const noexcept {
    switch (op) {
    case Op::Assign:
        return pushBytes(tx, "=", 1u);
    case Op::Add:
        return pushBytes(tx, "+=", 2u);
    case Op::Sub:
        return pushBytes(tx, "-=", 2u);
    case Op::Mul:
        return pushBytes(tx, "*=", 2u);
    case Op::Div:
        return pushBytes(tx, "/=", 2u);
    case Op::Mod:
        return pushBytes(tx, "%=", 2u);
    }
    return pushBytes(tx, "=", 1u);
}

bool Command::printAttrLexeme(TxFrame& tx, const AttrRef& t) const noexcept {
    if (t.attr.len == 0u)
        return fail(Status::EmptyAttribute);
    if (t.comp.len == 0u)
        return printLiteral(tx, t.attr);
    return printLiteral(tx, t.comp) && printDot(tx) && printLiteral(tx, t.attr);
}

bool Command::printColorConst(TxFrame& tx, Color::std c) const noexcept {
    const char* lex = nullptr;
    switch (c) {
    case Color::std::Black: lex = "BLACK"; break;
    case Color::std::White: lex = "WHITE"; break;
    case Color::std::Red: lex = "RED"; break;
    case Color::std::Green: lex = "GREEN"; break;
    case Color::std::Blue: lex = "BLUE"; break;
    case Color::std::Yellow: lex = "YELLOW"; break;
    case Color::std::Gray: lex = "GRAY"; break;
    case Color::std::Cyan: lex = "CYAN"; break;
    case Color::std::Magenta: lex = "MAGENTA"; break;
    case Color::std::Orange: lex = "ORANGE"; break;
    case Color::std::Purple: lex = "PURPLE"; break;
    case Color::std::Navy: lex = "NAVY"; break;
    case Color::std::Maroon: lex = "MAROON"; break;
    case Color::std::Silver: lex = "SILVER"; break;
    case Color::std::Olive: lex = "OLIVE"; break;
    case Color::std::Teal: lex = "TEAL"; break;
    default:
        return fail(Status::InvalidColor);
    }
    return printNisToken(tx, lex);
}

namespace misc {

void printTxPayloadLine(const char* label, const TxFrame& tx) noexcept {
    if (label != nullptr)
        NEX_DBG_TRACE_TX("%s", label);
    NEX_DBG_TRACE_TX("[%u] \"", static_cast<unsigned>(tx.length));
    for (uint16_t i = 0u; i < tx.length; ++i) {
        const uint8_t b = tx.payload[i];
        if (b >= 0x20u && b < 0x7Fu)
            NEX_DBG_TRACE_TX("%c", static_cast<char>(b));
        else
            NEX_DBG_TRACE_TX("\\x%02X", static_cast<unsigned>(b));
    }
    NEX_DBG_TRACE_TX("\" + 0xFF 0xFF 0xFF\n");
}

void printRxLine(const RxFrame& wire, const Message& parsed) noexcept {
    NEX_DBG_TRACE_RX_WIRE("RX wire hdr=0x%02X len=%u", static_cast<unsigned>(wire.header),
        static_cast<unsigned>(wire.length));
    if (wire.length > 0u) {
        NEX_DBG_TRACE_RX_WIRE(" [");
        for (uint16_t i = 0u; i < wire.length; ++i)
            NEX_DBG_TRACE_RX_WIRE("%s0x%02X", (i != 0u) ? " " : "",
                static_cast<unsigned>(wire.payload[i]));
        NEX_DBG_TRACE_RX_WIRE("]\n");
    } else {
        NEX_DBG_TRACE_RX_WIRE("\n");
    }

    std::visit([](auto&& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, msg::Status>) {
            NEX_DBG_TRACE_RX("RX msg Status %s (0x%02X)", cstr(m.status),
                static_cast<unsigned>(static_cast<uint8_t>(m.status)));
            if (m.tag_1 != 0u || m.tag_2 != 0u)
                NEX_DBG_TRACE_RX(" tag1=%u tag2=%u", static_cast<unsigned>(m.tag_1),
                    static_cast<unsigned>(m.tag_2));
        } else if constexpr (std::is_same_v<T, msg::getNumeric>) {
            NEX_DBG_TRACE_RX("RX msg getNumeric %ld", static_cast<long>(m.value));
        } else if constexpr (std::is_same_v<T, msg::getString>) {
            NEX_DBG_TRACE_RX("RX msg getString len=%u \"", static_cast<unsigned>(m.length));
            for (uint16_t i = 0u; i < m.length; ++i) {
                const uint8_t b = static_cast<uint8_t>(m.chars[i]);
                if (b >= 0x20u && b < 0x7Fu)
                    NEX_DBG_TRACE_RX("%c", static_cast<char>(b));
                else
                    NEX_DBG_TRACE_RX("\\x%02X", static_cast<unsigned>(b));
            }
            NEX_DBG_TRACE_RX("\"");
        } else if constexpr (std::is_same_v<T, msg::evTouch>) {
            NEX_DBG_TRACE_RX("RX msg evTouch page=%u comp=%u %s", static_cast<unsigned>(m.route.page),
                static_cast<unsigned>(m.route.comp),
                m.state == TouchState::Press ? "Press" : "Release");
        } else if constexpr (std::is_same_v<T, msg::evTouchXY>) {
            NEX_DBG_TRACE_RX("RX msg evTouchXY mode=0x%02X x=%u y=%u %s",
                static_cast<unsigned>(static_cast<uint8_t>(m.mode)), static_cast<unsigned>(m.pos.x),
                static_cast<unsigned>(m.pos.y), m.state == TouchState::Press ? "Press" : "Release");
        } else if constexpr (std::is_same_v<T, msg::evPage>) {
            NEX_DBG_TRACE_RX("RX msg evPage page=%u", static_cast<unsigned>(m.page));
        } else if constexpr (std::is_same_v<T, msg::evSystem>) {
            NEX_DBG_TRACE_RX("RX msg evSystem code=0x%02X", static_cast<unsigned>(static_cast<uint8_t>(m.code)));
        } else if constexpr (std::is_same_v<T, msg::evTransparent>) {
            NEX_DBG_TRACE_RX("RX msg evTransparent code=0x%02X", static_cast<unsigned>(static_cast<uint8_t>(m.code)));
        } else if constexpr (std::is_same_v<T, msg::evMsgBox>) {
            NEX_DBG_TRACE_RX("RX msg evMsgBox page=%u comp=%u tag=%u action=%u",
                static_cast<unsigned>(m.route.page), static_cast<unsigned>(m.route.comp),
                static_cast<unsigned>(m.tag), static_cast<unsigned>(static_cast<uint8_t>(m.action)));
        }
    }, parsed);
    NEX_DBG_TRACE_RX("\n");
}

} // namespace misc

namespace cmd {
namespace assign {

bool Text::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!printAttrLexeme(tx, _target))
        return false;
    const Command::Op numOp = (_op == Op::Append) ? Command::Op::Add : Command::Op::Assign;
    if (!printOperation(tx, numOp))
        return false;
    return printQuotedString(tx, _text);
}

bool TextSubtract::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!printAttrLexeme(tx, _target))
        return false;
    if (!printOperation(tx, Command::Op::Sub))
        return false;
    return printUint32(tx, _n);
}

bool Numeric::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!printAttrLexeme(tx, _target))
        return false;
    if (!printOperation(tx, _op))
        return false;
    return printInt32(tx, _value);
}

} // namespace assign


// --- NIS §3 operational ---

bool System::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    switch (_kind) {
    case Kind::TouchJ:
        return NEX_CMD_PRINT_LIT(tx, "touch_j");
    case Kind::Restart:
        return NEX_CMD_PRINT_LIT(tx, "rest");
    default:
        return fail(Status::UnknownKind);
    }
}

bool Page::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    switch (_kind) {
    case Kind::SendMe:
        return NEX_CMD_PRINT_LIT(tx, "sendme");
    case Kind::Switch:
        if (!NEX_CMD_PRINT_LIT(tx, "page") || !printSpace(tx))
            return false;
        return printUint32(tx, _pageId);
    case Kind::SwitchByName:
        if (!NEX_CMD_PRINT_LIT(tx, "page") || !printSpace(tx))
            return false;
        return _pageName != nullptr && printLiteral(tx, *_pageName);
    case Kind::Refresh:
        if (!NEX_CMD_PRINT_LIT(tx, "ref") || !printSpace(tx))
            return false;
        return printUint32(tx, 0u);
    default:
        return fail(Status::UnknownKind);
    }
}

bool Get::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "get") || !printSpace(tx))
        return false;
    return printAttrLexeme(tx, _operand);
}

bool String::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::Covx) {
        if (!NEX_CMD_PRINT_LIT(tx, "covx") || !printSpace(tx))
            return false;
        return printAttrLexeme(tx, _a1) && printComma(tx) && printAttrLexeme(tx, _a2)
            && printComma(tx) && printInt32(tx, _i1) && printComma(tx)
            && printInt32(tx, _i2);
    }

    if (_kind == Kind::Substr) {
        if (!NEX_CMD_PRINT_LIT(tx, "substr") || !printSpace(tx))
            return false;
        return printAttrLexeme(tx, _a1) && printComma(tx) && printAttrLexeme(tx, _a2)
            && printComma(tx) && printUint32(tx, static_cast<uint32_t>(_i1)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(_i2));
    }

    if (_kind == Kind::Strlen) {
        if (!NEX_CMD_PRINT_LIT(tx, "strlen") || !printSpace(tx))
            return false;
        return printAttrLexeme(tx, _a1) && printComma(tx) && printAttrLexeme(tx, _a2);
    }

    if (_kind == Kind::Btlen) {
        if (!NEX_CMD_PRINT_LIT(tx, "btlen") || !printSpace(tx))
            return false;
        return printAttrLexeme(tx, _a1) && printComma(tx) && printAttrLexeme(tx, _a2);
    }

    if (_kind != Kind::Spstr)
        return fail(Status::UnknownKind);

    if (_delimiterQuoted == nullptr)
        return fail(Status::NullPointer);
    if (!NEX_CMD_PRINT_LIT(tx, "spstr") || !printSpace(tx))
        return false;
    const size_t ndq = std::strlen(_delimiterQuoted);
    if (ndq == 0u)
        return fail(Status::EmptyLiteral);
    if (ndq > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return printAttrLexeme(tx, _a1) && printComma(tx) && printAttrLexeme(tx, _a2)
        && printComma(tx) && pushBytes(tx, _delimiterQuoted, static_cast<uint16_t>(ndq))
        && printComma(tx) && printUint32(tx, static_cast<uint32_t>(_i1));
}

bool Component::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::Refresh) {
        if (!NEX_CMD_PRINT_LIT(tx, "ref") || !printSpace(tx))
            return false;
        return printLiteral(tx, _compName);
    }

    if (_kind == Kind::Setlayer) {
        if (!NEX_CMD_PRINT_LIT(tx, "setlayer") || !printSpace(tx))
            return false;
        if (_arg.aboveCompNameOr255 == nullptr)
            return fail(Status::NullPointer);
        return printLiteral(tx, _compName) && printComma(tx)
            && printLiteral(tx, *_arg.aboveCompNameOr255);
    }

    if (_kind == Kind::Visible) {
        if (!NEX_CMD_PRINT_LIT(tx, "vis") || !printSpace(tx))
            return false;
    } else if (_kind == Kind::TouchSwitch) {
        if (!NEX_CMD_PRINT_LIT(tx, "tsw") || !printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "click") || !printSpace(tx))
            return false;
    }
    return printLiteral(tx, _compName) && printComma(tx) && printUint32(tx, _arg.arg01);
}

bool WaveForm::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::RefreshStart)
        return NEX_CMD_PRINT_LIT(tx, "ref_start");

    if (_kind == Kind::RefreshStop)
        return NEX_CMD_PRINT_LIT(tx, "ref_stop");

    if (_kind == Kind::Add) {
        if (!NEX_CMD_PRINT_LIT(tx, "add") || !printSpace(tx))
            return false;
        return printUint32(tx, _waveformId) && printComma(tx) && printUint32(tx, _arg1)
            && printComma(tx) && printUint32(tx, _arg2);
    }

    if (_kind == Kind::AddT) {
        if (!NEX_CMD_PRINT_LIT(tx, "addt") || !printSpace(tx))
            return false;
        return printUint32(tx, _waveformId) && printComma(tx) && printUint32(tx, _arg1)
            && printComma(tx) && printUint32(tx, _arg2);
    }

    if (!NEX_CMD_PRINT_LIT(tx, "cle") || !printSpace(tx))
        return false;
    return printUint32(tx, _waveformId) && printComma(tx) && printUint32(tx, _arg1);
}

bool Eeprom::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_target != nullptr) {
        if (_op == Op::Write) {
            if (!NEX_CMD_PRINT_LIT(tx, "wepo") || !printSpace(tx))
                return false;
        } else {
            if (!NEX_CMD_PRINT_LIT(tx, "repo") || !printSpace(tx))
                return false;
        }
        return printAttrLexeme(tx, *_target) && printComma(tx) && printUint32(tx, _addr);
    }

    if (_target != nullptr || _byteCount == 0u)
        return fail(Status::InvalidFields);
    if (_op == Op::Write) {
        if (!NEX_CMD_PRINT_LIT(tx, "wept") || !printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "rept") || !printSpace(tx))
            return false;
    }
    return printUint32(tx, _addr) && printComma(tx) && printUint32(tx, _byteCount);
}

bool Cfgpio::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "cfgpio") || !printSpace(tx))
        return false;
    return printUint32(tx, _pin) && printComma(tx) && printUint32(tx, _mode)
        && printComma(tx) && printLiteral(tx, _bindCompNameOrZero);
}

bool Move::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "move") || !printSpace(tx))
        return false;
    return printLiteral(tx, _compName) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_from.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_from.y)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_to.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_to.y)) && printComma(tx)
        && printUint32(tx, _priority) && printComma(tx) && printUint32(tx, _timeMs);
}

bool Play::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "play") || !printSpace(tx))
        return false;
    return printUint32(tx, _channel) && printComma(tx) && printUint32(tx, _resourceId)
        && printComma(tx) && printUint32(tx, _loop01);
}

bool File::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_pathQuoted == nullptr)
        return fail(Status::NullPointer);

    if (_op == Op::Delete) {
        if (!NEX_CMD_PRINT_LIT(tx, "delfile") || !printSpace(tx))
            return false;
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(n));
    }

    if (_op == Op::Rename) {
        if (_pathToQuoted == nullptr)
            return fail(Status::NullPointer);
        if (!NEX_CMD_PRINT_LIT(tx, "refile") || !printSpace(tx))
            return false;
        const size_t na = std::strlen(_pathQuoted);
        const size_t nb = std::strlen(_pathToQuoted);
        if (na == 0u || nb == 0u)
            return fail(Status::EmptyLiteral);
        if (na > UINT16_MAX || nb > UINT16_MAX)
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(na)) && printComma(tx)
            && pushBytes(tx, _pathToQuoted, static_cast<uint16_t>(nb));
    }

    if (_op == Op::Create) {
        if (!NEX_CMD_PRINT_LIT(tx, "newfile") || !printSpace(tx))
            return false;
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(n)) && printComma(tx)
            && printUint32(tx, _reservedSize);
    }

    if (_op == Op::Read) {
        if (!NEX_CMD_PRINT_LIT(tx, "rdfile") || !printSpace(tx))
            return false;
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(n)) && printComma(tx)
            && printUint32(tx, _offset) && printComma(tx) && printUint32(tx, _count)
            && printComma(tx) && printUint32(tx, _crcOption);
    }

    if (_op == Op::WriteT) {
        if (!NEX_CMD_PRINT_LIT(tx, "twfile") || !printSpace(tx))
            return false;
        const size_t np = std::strlen(_pathQuoted);
        if (np == 0u)
            return fail(Status::EmptyLiteral);
        if (np > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(np)) && printComma(tx)
            && printUint32(tx, _reservedSize);
    }

    if (_dstNumAttr == nullptr)
        return fail(Status::NullPointer);
    if (!NEX_CMD_PRINT_LIT(tx, "findfile") || !printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u)
        return fail(Status::EmptyLiteral);
    if (n > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(n)) && printComma(tx)
        && printAttrLexeme(tx, *_dstNumAttr);
    return fail(Status::UnknownKind);
}

bool Directory::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_pathQuoted == nullptr)
        return fail(Status::NullPointer);

    if (_op == Op::Delete || _op == Op::Create) {
        if (_op == Op::Delete) {
            if (!NEX_CMD_PRINT_LIT(tx, "deldir") || !printSpace(tx))
                return false;
        } else {
            if (!NEX_CMD_PRINT_LIT(tx, "newdir") || !printSpace(tx))
                return false;
        }
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(n));
    }

    if (_op == Op::Rename) {
        if (_pathToQuoted == nullptr)
            return fail(Status::NullPointer);
        if (!NEX_CMD_PRINT_LIT(tx, "redir") || !printSpace(tx))
            return false;
        const size_t na = std::strlen(_pathQuoted);
        const size_t nb = std::strlen(_pathToQuoted);
        if (na == 0u || nb == 0u)
            return fail(Status::EmptyLiteral);
        if (na > UINT16_MAX || nb > UINT16_MAX)
            return fail(Status::TxFrameOverflow);
        return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(na)) && printComma(tx)
            && pushBytes(tx, _pathToQuoted, static_cast<uint16_t>(nb));
    }

    if (_dstNumAttr == nullptr)
        return fail(Status::NullPointer);
    if (!NEX_CMD_PRINT_LIT(tx, "finddir") || !printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u)
        return fail(Status::EmptyLiteral);
    if (n > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return pushBytes(tx, _pathQuoted, static_cast<uint16_t>(n)) && printComma(tx)
        && printAttrLexeme(tx, *_dstNumAttr);
    return fail(Status::UnknownKind);
}

bool FileStream::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;

    if (_kind == Kind::Close) {
        if (!NEX_CMD_PRINT_LIT(tx, "close") || !printSpace(tx))
            return false;
        return printLiteral(tx, _compName);
    }

    if (_kind == Kind::Read) {
        if (!NEX_CMD_PRINT_LIT(tx, "read") || !printSpace(tx))
            return false;
        return printLiteral(tx, _compName) && printComma(tx) && printUint32(tx, _arg1)
            && printComma(tx) && printUint32(tx, _arg2);
    }

    if (_kind == Kind::Write) {
        if (!NEX_CMD_PRINT_LIT(tx, "write") || !printSpace(tx))
            return false;
        return printLiteral(tx, _compName) && printComma(tx) && printUint32(tx, _arg1);
    }

    if (_path == nullptr)
        return fail(Status::NullPointer);

    if (_kind == Kind::Open) {
        if (!NEX_CMD_PRINT_LIT(tx, "open") || !printSpace(tx))
            return false;
        return printLiteral(tx, _compName) && printComma(tx) && printQuotedString(tx, _path);
    }

    if (_kind == Kind::Find) {
        if (!NEX_CMD_PRINT_LIT(tx, "find") || !printSpace(tx))
            return false;
        return printLiteral(tx, _compName) && printComma(tx) && printQuotedString(tx, _path);
    }

    return fail(Status::UnknownKind);
}

namespace gui {

bool ClearScreen::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "cls") || !printSpace(tx))
        return false;
    return printUint32(tx, static_cast<uint32_t>(_color.raw));
}

bool Picture::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "pic") || !printSpace(tx))
        return false;
    return printInt32(tx, static_cast<int32_t>(_at.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_at.y)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_pictureId));
}

bool PictureCrop::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_mode == Mode::InPlace) {
        if (_region.size.w == 0u || _region.size.h == 0u)
            return fail(Status::InvalidGeometry);
        if (!NEX_CMD_PRINT_LIT(tx, "picq") || !printSpace(tx))
            return false;
        return printInt32(tx, static_cast<int32_t>(_region.ul.x)) && printComma(tx)
            && printInt32(tx, static_cast<int32_t>(_region.ul.y)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(_region.size.w)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(_region.size.h)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(_pictureId));
    }

    if (_region.size.w == 0u || _region.size.h == 0u)
        return fail(Status::InvalidGeometry);

    if (!NEX_CMD_PRINT_LIT(tx, "xpic") || !printSpace(tx))
        return false;
    return printInt32(tx, static_cast<int32_t>(_dst.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_dst.y)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_region.size.w)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_region.size.h)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_region.ul.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_region.ul.y)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_pictureId));
}

bool TextInRegion::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "xstr") || !printSpace(tx))
        return false;
    if (_contentToken == nullptr)
        return fail(Status::NullPointer);
    return printInt32(tx, static_cast<int32_t>(_region.ul.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_region.ul.y)) && printComma(tx)
        && printUint32(tx, _region.size.w) && printComma(tx) && printUint32(tx, _region.size.h)
        && printComma(tx) && printUint32(tx, _fontId) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_fg.raw)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_bg.raw)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(static_cast<uint8_t>(_hAlign))) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(static_cast<uint8_t>(_vAlign)))
        && printComma(tx) && printUint32(tx, static_cast<uint32_t>(static_cast<uint8_t>(_fill)))
        && printComma(tx) && printQuotedString(tx, _contentToken);
}

bool Rect::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    const int32_t w = static_cast<int32_t>(_region.size.w);
    const int32_t h = static_cast<int32_t>(_region.size.h);
    if (w <= 0 || h <= 0)
        return fail(Status::InvalidGeometry);

    const Point lr = _region.lowerRight();
    if (_mode == Mode::Fill) {
        if (!NEX_CMD_PRINT_LIT(tx, "fill") || !printSpace(tx))
            return false;
        return printInt32(tx, static_cast<int32_t>(_region.ul.x)) && printComma(tx)
            && printInt32(tx, static_cast<int32_t>(_region.ul.y)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(w)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(h)) && printComma(tx)
            && printUint32(tx, static_cast<uint32_t>(_color.raw));
    }
    if (!NEX_CMD_PRINT_LIT(tx, "draw") || !printSpace(tx))
        return false;
    return printInt32(tx, static_cast<int32_t>(_region.ul.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_region.ul.y)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(lr.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(lr.y)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_color.raw));
}

bool Line::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, "line") || !printSpace(tx))
        return false;
    return printInt32(tx, static_cast<int32_t>(_from.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_from.y)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_to.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_to.y)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_color.raw));
}

bool Circle::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::Filled) {
        if (!NEX_CMD_PRINT_LIT(tx, "cirs") || !printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "cir") || !printSpace(tx))
            return false;
    }
    return printInt32(tx, static_cast<int32_t>(_center.x)) && printComma(tx)
        && printInt32(tx, static_cast<int32_t>(_center.y)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_radius)) && printComma(tx)
        && printUint32(tx, static_cast<uint32_t>(_color.raw));
}

} // namespace gui

} // namespace cmd

bool EmptyCommand::serialize(TxFrame& tx) const noexcept {
    tx.length = 0u;
    return true;
}

bool EmptyCommand::emplaceIn(void* storage, std::size_t maxBytes, std::size_t maxAlign) const noexcept {
    (void)storage;
    (void)maxBytes;
    (void)maxAlign;
    return fail(Status::InvalidFields);
}

void EmptyCommand::destroyIn(void* storage) const noexcept {
    (void)storage;
}

const EmptyCommand& kEmptyCommand() noexcept {
    static const EmptyCommand instance{};
    return instance;
}

} // namespace nex

#undef NEX_CMD_PRINT_LIT
