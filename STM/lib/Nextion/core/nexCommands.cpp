#include "nexCommands.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

/** Литерал имени команды NIS в кадр; длина — `sizeof(lit) - 1` (без strlen). `lit` — только строковый литерал `"…"`. */
#define NEX_CMD_PRINT_LIT(tx, st, lit) (misc::pushBytes((tx), (st), (lit), static_cast<uint16_t>(sizeof(lit) - 1u)))

namespace nex {

namespace misc {

bool pushBytes(TxFrame& tx, Command::Status& st, const void* src, uint16_t n) noexcept {
    if (n == 0u)
        return true;
    if (src == nullptr) {
        st = Command::Status::NullPointer;
        return false;
    }
    if (static_cast<uint32_t>(tx.length) + static_cast<uint32_t>(n) > static_cast<uint32_t>(TxFrame::MAX_PAYLOAD)) {
        st = Command::Status::TxFrameOverflow;
        return false;
    }
    std::memcpy(tx.payload + tx.length, src, n);
    tx.length = static_cast<uint16_t>(tx.length + n);
    return true;
}

/** Печать `nex::Literal` в кадр (сырой ASCII, ровно `len` байт с `data`); `nullptr` или `len == 0` — ошибка. */
bool printLiteral(TxFrame& tx, Command::Status& st, const Literal& lit) noexcept {
    if (lit.data == nullptr) {
        st = Command::Status::NullPointer;
        return false;
    }
    if (lit.len == 0u) {
        st = Command::Status::EmptyLiteral;
        return false;
    }
    return pushBytes(tx, st, lit.data, lit.len);
}

/** Печать ASCIIZ-токена NIS в кадр как сырые байты (длина через `strlen`). */
bool printNisToken(TxFrame& tx, Command::Status& st, const char* token) noexcept {
    if (token == nullptr) {
        st = Command::Status::NullPointer;
        return false;
    }
    const size_t n = std::strlen(token);
    if (n == 0u) {
        st = Command::Status::EmptyLiteral;
        return false;
    }
    if (n > static_cast<size_t>(UINT16_MAX)) {
        st = Command::Status::TxFrameOverflow;
        return false;
    }
    return pushBytes(tx, st, token, static_cast<uint16_t>(n));
}

bool printSpace(TxFrame& tx, Command::Status& st) noexcept {
    const uint8_t sp = static_cast<uint8_t>(' ');
    return pushBytes(tx, st, &sp, 1u);
}

bool printDot(TxFrame& tx, Command::Status& st) noexcept {
    const uint8_t dot = static_cast<uint8_t>('.');
    return pushBytes(tx, st, &dot, 1u);
}

bool printComma(TxFrame& tx, Command::Status& st) noexcept {
    const uint8_t comma = static_cast<uint8_t>(',');
    return pushBytes(tx, st, &comma, 1u);
}

bool printQuotedString(TxFrame& tx, Command::Status& st, const char* text) noexcept {
    const uint8_t dq = static_cast<uint8_t>('"');
    if (!pushBytes(tx, st, &dq, 1u))
        return false;
    if (text == nullptr)
        return pushBytes(tx, st, &dq, 1u);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0u; ++p) {
        const unsigned char c = *p;
        if (c == static_cast<unsigned char>('"')) {
            static const char esc[] = "\\\"";
            if (!pushBytes(tx, st, esc, 2u))
                return false;
        } else if (c == static_cast<unsigned char>('\\')) {
            static const char esc[] = "\\\\";
            if (!pushBytes(tx, st, esc, 2u))
                return false;
        } else if (c == static_cast<unsigned char>('\r')) {
            static const char esc[] = "\\r";
            if (!pushBytes(tx, st, esc, 2u))
                return false;
        } else if (!pushBytes(tx, st, &c, 1u)) {
            return false;
        }
    }
    return pushBytes(tx, st, &dq, 1u);
}

bool printUint32(TxFrame& tx, Command::Status& st, uint32_t value) noexcept {
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
    return pushBytes(tx, st, digits, n);
}

bool printInt32(TxFrame& tx, Command::Status& st, int32_t value) noexcept {
    if (value >= 0)
        return printUint32(tx, st, static_cast<uint32_t>(value));
    const uint8_t minus = static_cast<uint8_t>('-');
    if (!pushBytes(tx, st, &minus, 1u))
        return false;
    uint32_t u = static_cast<uint32_t>(value);
    u = ~u + 1u;
    return printUint32(tx, st, u);
}

bool printOperation(TxFrame& tx, Command::Status& st, cmd::assign::Numeric::Op op) noexcept {
    using Op = cmd::assign::Numeric::Op;
    switch (op) {
    case Op::Assign:
        return pushBytes(tx, st, "=", 1u);
    case Op::Add:
        return pushBytes(tx, st, "+=", 2u);
    case Op::Sub:
        return pushBytes(tx, st, "-=", 2u);
    case Op::Mul:
        return pushBytes(tx, st, "*=", 2u);
    case Op::Div:
        return pushBytes(tx, st, "/=", 2u);
    case Op::Mod:
        return pushBytes(tx, st, "%=", 2u);
    }
    return pushBytes(tx, st, "=", 1u);
}

bool printAttrLexeme(TxFrame& tx, Command::Status& st, const cmd::TargetAttr& t) noexcept {
    if (t.attr.len == 0u) {
        st = Command::Status::EmptyAttribute;
        return false;
    }
    if (t.comp.len == 0u)
        return printLiteral(tx, st, t.attr);
    return printLiteral(tx, st, t.comp) && printDot(tx, st) && printLiteral(tx, st, t.attr);
}

bool printColorConst(TxFrame& tx, Command::Status& st, Color::std c) noexcept {
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
        st = Command::Status::InvalidColor;
        return false;
    }
    return printNisToken(tx, st, lex);
}

void printTxPayloadLine(const char* label, const TxFrame& tx) noexcept {
    if (label != nullptr)
        std::printf("%s", label);
    std::printf("\"%.*s\" + 0xFF 0xFF 0xFF\n", static_cast<int>(tx.length),
        reinterpret_cast<const char*>(tx.payload));
}

} // namespace misc

namespace cmd {
namespace assign {

bool Text::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!misc::printAttrLexeme(tx, _status, _target))
        return false;
    using NO = Numeric::Op;
    const NO numOp = (_op == Op::Append) ? NO::Add : NO::Assign;
    if (!misc::printOperation(tx, _status, numOp))
        return false;
    return misc::printQuotedString(tx, _status, _text);
}

bool TextSubtract::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!misc::printAttrLexeme(tx, _status, _target))
        return false;
    if (!misc::printOperation(tx, _status, Numeric::Op::Sub))
        return false;
    return misc::printUint32(tx, _status, _n);
}

bool Numeric::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!misc::printAttrLexeme(tx, _status, _target))
        return false;
    if (!misc::printOperation(tx, _status, _op))
        return false;
    return misc::printInt32(tx, _status, _value);
}

} // namespace assign


// --- NIS §3 operational ---

bool System::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    switch (_kind) {
    case Kind::TouchJ:
        return NEX_CMD_PRINT_LIT(tx, _status, "touch_j");
    case Kind::Restart:
        return NEX_CMD_PRINT_LIT(tx, _status, "rest");
    case Kind::Randset:
        if (!NEX_CMD_PRINT_LIT(tx, _status, "randset") || !misc::printSpace(tx, _status))
            return false;
        return misc::printInt32(tx, _status, _a) && misc::printComma(tx, _status) && misc::printInt32(tx, _status, _b);
    default:
        return fail(Status::UnknownKind);
    }
}

bool Get::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "get") || !misc::printSpace(tx, _status))
        return false;
    return misc::printAttrLexeme(tx, _status, _operand);
}

bool String::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::Covx) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "covx") || !misc::printSpace(tx, _status))
            return false;
        return misc::printAttrLexeme(tx, _status, _a1) && misc::printComma(tx, _status) && misc::printAttrLexeme(tx, _status, _a2)
            && misc::printComma(tx, _status) && misc::printInt32(tx, _status, _i1) && misc::printComma(tx, _status)
            && misc::printInt32(tx, _status, _i2);
    }

    if (_kind == Kind::Substr) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "substr") || !misc::printSpace(tx, _status))
            return false;
        return misc::printAttrLexeme(tx, _status, _a1) && misc::printComma(tx, _status) && misc::printAttrLexeme(tx, _status, _a2)
            && misc::printComma(tx, _status) && misc::printUint32(tx, _status, static_cast<uint32_t>(_i1)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, static_cast<uint32_t>(_i2));
    }

    if (_kind == Kind::Strlen) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "strlen") || !misc::printSpace(tx, _status))
            return false;
        return misc::printAttrLexeme(tx, _status, _a1) && misc::printComma(tx, _status) && misc::printAttrLexeme(tx, _status, _a2);
    }

    if (_kind == Kind::Btlen) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "btlen") || !misc::printSpace(tx, _status))
            return false;
        return misc::printAttrLexeme(tx, _status, _a1) && misc::printComma(tx, _status) && misc::printAttrLexeme(tx, _status, _a2);
    }

    if (_kind != Kind::Spstr)
        return fail(Status::UnknownKind);

    if (_delimiterQuoted == nullptr)
        return fail(Status::NullPointer);
    if (!NEX_CMD_PRINT_LIT(tx, _status, "spstr") || !misc::printSpace(tx, _status))
        return false;
    const size_t ndq = std::strlen(_delimiterQuoted);
    if (ndq == 0u)
        return fail(Status::EmptyLiteral);
    if (ndq > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return misc::printAttrLexeme(tx, _status, _a1) && misc::printComma(tx, _status) && misc::printAttrLexeme(tx, _status, _a2)
        && misc::printComma(tx, _status) && misc::pushBytes(tx, _status, _delimiterQuoted, static_cast<uint16_t>(ndq))
        && misc::printComma(tx, _status) && misc::printUint32(tx, _status, static_cast<uint32_t>(_i1));
}

bool Component::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::Refresh) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "ref") || !misc::printSpace(tx, _status))
            return false;
        return misc::printLiteral(tx, _status, _compName);
    }

    if (_kind == Kind::Setlayer) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "setlayer") || !misc::printSpace(tx, _status))
            return false;
        if (_arg.aboveCompNameOr255 == nullptr)
            return fail(Status::NullPointer);
        return misc::printLiteral(tx, _status, _compName) && misc::printComma(tx, _status)
            && misc::printLiteral(tx, _status, *_arg.aboveCompNameOr255);
    }

    if (_kind == Kind::Visible) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "vis") || !misc::printSpace(tx, _status))
            return false;
    } else if (_kind == Kind::TouchSwitch) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "tsw") || !misc::printSpace(tx, _status))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "click") || !misc::printSpace(tx, _status))
            return false;
    }
    return misc::printLiteral(tx, _status, _compName) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _arg.arg01);
}

bool WaveForm::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::RefreshStart)
        return NEX_CMD_PRINT_LIT(tx, _status, "ref_start");

    if (_kind == Kind::RefreshStop)
        return NEX_CMD_PRINT_LIT(tx, _status, "ref_stop");

    if (_kind == Kind::Add) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "add") || !misc::printSpace(tx, _status))
            return false;
        return misc::printUint32(tx, _status, _waveformId) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _arg1)
            && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _arg2);
    }

    if (_kind == Kind::AddT) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "addt") || !misc::printSpace(tx, _status))
            return false;
        return misc::printUint32(tx, _status, _waveformId) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _arg1)
            && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _arg2);
    }

    if (!NEX_CMD_PRINT_LIT(tx, _status, "cle") || !misc::printSpace(tx, _status))
        return false;
    return misc::printUint32(tx, _status, _waveformId) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _arg1);
}

bool Eeprom::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_target != nullptr) {
        if (_op == Op::Write) {
            if (!NEX_CMD_PRINT_LIT(tx, _status, "wepo") || !misc::printSpace(tx, _status))
                return false;
        } else {
            if (!NEX_CMD_PRINT_LIT(tx, _status, "repo") || !misc::printSpace(tx, _status))
                return false;
        }
        return misc::printAttrLexeme(tx, _status, *_target) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _addr);
    }

    if (_target != nullptr || _byteCount == 0u)
        return fail(Status::InvalidFields);
    if (_op == Op::Write) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "wept") || !misc::printSpace(tx, _status))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "rept") || !misc::printSpace(tx, _status))
            return false;
    }
    return misc::printUint32(tx, _status, _addr) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _byteCount);
}

bool Cfgpio::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "cfgpio") || !misc::printSpace(tx, _status))
        return false;
    return misc::printUint32(tx, _status, _pin) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _mode)
        && misc::printComma(tx, _status) && misc::printLiteral(tx, _status, _bindCompNameOrZero);
}

bool Move::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "move") || !misc::printSpace(tx, _status))
        return false;
    return misc::printLiteral(tx, _status, _compName) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_from.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_from.y)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_to.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_to.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, _priority) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _timeMs);
}

bool Play::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "play") || !misc::printSpace(tx, _status))
        return false;
    return misc::printUint32(tx, _status, _channel) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _resourceId)
        && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _loop01);
}

bool File::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_pathQuoted == nullptr)
        return fail(Status::NullPointer);

    if (_op == Op::Delete) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "delfile") || !misc::printSpace(tx, _status))
            return false;
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(n));
    }

    if (_op == Op::Rename) {
        if (_pathToQuoted == nullptr)
            return fail(Status::NullPointer);
        if (!NEX_CMD_PRINT_LIT(tx, _status, "refile") || !misc::printSpace(tx, _status))
            return false;
        const size_t na = std::strlen(_pathQuoted);
        const size_t nb = std::strlen(_pathToQuoted);
        if (na == 0u || nb == 0u)
            return fail(Status::EmptyLiteral);
        if (na > UINT16_MAX || nb > UINT16_MAX)
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(na)) && misc::printComma(tx, _status)
            && misc::pushBytes(tx, _status, _pathToQuoted, static_cast<uint16_t>(nb));
    }

    if (_op == Op::Create) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "newfile") || !misc::printSpace(tx, _status))
            return false;
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(n)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, _reservedSize);
    }

    if (_op == Op::Read) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "rdfile") || !misc::printSpace(tx, _status))
            return false;
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(n)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, _offset) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _count)
            && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _crcOption);
    }

    if (_op == Op::WriteT) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "twfile") || !misc::printSpace(tx, _status))
            return false;
        const size_t np = std::strlen(_pathQuoted);
        if (np == 0u)
            return fail(Status::EmptyLiteral);
        if (np > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(np)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, _reservedSize);
    }

    if (_dstNumAttr == nullptr)
        return fail(Status::NullPointer);
    if (!NEX_CMD_PRINT_LIT(tx, _status, "findfile") || !misc::printSpace(tx, _status))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u)
        return fail(Status::EmptyLiteral);
    if (n > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(n)) && misc::printComma(tx, _status)
        && misc::printAttrLexeme(tx, _status, *_dstNumAttr);
    return fail(Status::UnknownKind);
}

bool Directory::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_pathQuoted == nullptr)
        return fail(Status::NullPointer);

    if (_op == Op::Delete || _op == Op::Create) {
        if (_op == Op::Delete) {
            if (!NEX_CMD_PRINT_LIT(tx, _status, "deldir") || !misc::printSpace(tx, _status))
                return false;
        } else {
            if (!NEX_CMD_PRINT_LIT(tx, _status, "newdir") || !misc::printSpace(tx, _status))
                return false;
        }
        const size_t n = std::strlen(_pathQuoted);
        if (n == 0u)
            return fail(Status::EmptyLiteral);
        if (n > static_cast<size_t>(UINT16_MAX))
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(n));
    }

    if (_op == Op::Rename) {
        if (_pathToQuoted == nullptr)
            return fail(Status::NullPointer);
        if (!NEX_CMD_PRINT_LIT(tx, _status, "redir") || !misc::printSpace(tx, _status))
            return false;
        const size_t na = std::strlen(_pathQuoted);
        const size_t nb = std::strlen(_pathToQuoted);
        if (na == 0u || nb == 0u)
            return fail(Status::EmptyLiteral);
        if (na > UINT16_MAX || nb > UINT16_MAX)
            return fail(Status::TxFrameOverflow);
        return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(na)) && misc::printComma(tx, _status)
            && misc::pushBytes(tx, _status, _pathToQuoted, static_cast<uint16_t>(nb));
    }

    if (_dstNumAttr == nullptr)
        return fail(Status::NullPointer);
    if (!NEX_CMD_PRINT_LIT(tx, _status, "finddir") || !misc::printSpace(tx, _status))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u)
        return fail(Status::EmptyLiteral);
    if (n > static_cast<size_t>(UINT16_MAX))
        return fail(Status::TxFrameOverflow);
    return misc::pushBytes(tx, _status, _pathQuoted, static_cast<uint16_t>(n)) && misc::printComma(tx, _status)
        && misc::printAttrLexeme(tx, _status, *_dstNumAttr);
    return fail(Status::UnknownKind);
}

namespace gui {

bool ClearScreen::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "cls") || !misc::printSpace(tx, _status))
        return false;
    return misc::printUint32(tx, _status, static_cast<uint32_t>(_color.raw));
}

bool Picture::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "pic") || !misc::printSpace(tx, _status))
        return false;
    return misc::printInt32(tx, _status, static_cast<int32_t>(_at.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_at.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_pictureId));
}

bool PictureCrop::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_mode == Mode::InPlace) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "picq") || !misc::printSpace(tx, _status))
            return false;
        return misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx, _status)
            && misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, _w) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _h)
            && misc::printComma(tx, _status) && misc::printUint32(tx, _status, static_cast<uint32_t>(_pictureId));
    }

    if (!NEX_CMD_PRINT_LIT(tx, _status, "xpic") || !misc::printSpace(tx, _status))
        return false;
    return misc::printInt32(tx, _status, static_cast<int32_t>(_dst.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_dst.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, _w) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _h)
        && misc::printComma(tx, _status) && misc::printInt32(tx, _status, static_cast<int32_t>(_src.x))
        && misc::printComma(tx, _status) && misc::printInt32(tx, _status, static_cast<int32_t>(_src.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_pictureId));
}

bool TextInRegion::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "xstr") || !misc::printSpace(tx, _status))
        return false;
    if (_contentToken == nullptr)
        return fail(Status::NullPointer);
    return misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, _w) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _h)
        && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _fontId) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_fg.raw)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_bg.raw)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, _hAlign) && misc::printComma(tx, _status) && misc::printUint32(tx, _status, _vAlign)
        && misc::printComma(tx, _status) && misc::printUint32(tx, _status, static_cast<uint32_t>(static_cast<uint8_t>(_fill)))
        && misc::printComma(tx, _status) && misc::printQuotedString(tx, _status, _contentToken);
}

bool Rect::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    const int32_t w = static_cast<int32_t>(_lowerRight.x) - static_cast<int32_t>(_upperLeft.x) + 1;
    const int32_t h = static_cast<int32_t>(_lowerRight.y) - static_cast<int32_t>(_upperLeft.y) + 1;
    if (w <= 0 || h <= 0)
        return fail(Status::InvalidGeometry);

    if (_mode == Mode::Fill) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "fill") || !misc::printSpace(tx, _status))
            return false;
        return misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx, _status)
            && misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, static_cast<uint32_t>(w)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, static_cast<uint32_t>(h)) && misc::printComma(tx, _status)
            && misc::printUint32(tx, _status, static_cast<uint32_t>(_color.raw));
    }
    if (!NEX_CMD_PRINT_LIT(tx, _status, "draw") || !misc::printSpace(tx, _status))
        return false;
    return misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_lowerRight.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_lowerRight.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_color.raw));
}

bool Line::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (!NEX_CMD_PRINT_LIT(tx, _status, "line") || !misc::printSpace(tx, _status))
        return false;
    return misc::printInt32(tx, _status, static_cast<int32_t>(_from.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_from.y)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_to.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_to.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_color.raw));
}

bool Circle::serialize(TxFrame& tx) const noexcept {
    _status = Status::OK;
    if (_kind == Kind::Filled) {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "cirs") || !misc::printSpace(tx, _status))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, _status, "cir") || !misc::printSpace(tx, _status))
            return false;
    }
    return misc::printInt32(tx, _status, static_cast<int32_t>(_center.x)) && misc::printComma(tx, _status)
        && misc::printInt32(tx, _status, static_cast<int32_t>(_center.y)) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, _radius) && misc::printComma(tx, _status)
        && misc::printUint32(tx, _status, static_cast<uint32_t>(_color.raw));
}

} // namespace gui

} // namespace cmd

} // namespace nex

#undef NEX_CMD_PRINT_LIT
