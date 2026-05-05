#include "nexCommands.hpp"
#include <cstdint>
#include <cstring>

namespace {
bool printNisToken(nex::TxFrame& tx, const char* token) noexcept {
    if (token == nullptr)
        return false;
    const size_t n = std::strlen(token);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(token, static_cast<uint16_t>(n));
}
} // namespace

/** Литерал имени команды NIS в кадр; длина — `sizeof(lit) - 1` (без strlen). `lit` — только строковый литерал `"…"`. */
#define NEX_CMD_PRINT_LIT(tx, lit) ((tx).pushBytes((lit), static_cast<uint16_t>(sizeof(lit) - 1u)))

namespace nex {
namespace misc {

/** Печать `nex::Literal` в кадр (сырой ASCII, ровно `len` байт с `data`); `nullptr` или `len == 0` — ошибка. */
bool printLiteral(TxFrame& tx, const Literal& lit) noexcept {
    if (lit.data == nullptr || lit.len == 0u)
        return false;
    return tx.pushBytes(lit.data, lit.len);
}

/** Печать одного пробела U+0020 между лексемами команды.
 * @param tx буфер исходящего кадра. */
bool printSpace(TxFrame& tx) noexcept {
    const uint8_t sp = static_cast<uint8_t>(' ');
    return tx.pushBytes(&sp, 1u);
}

/** Печать точки между именем страницы и именем компонента в лексеме цели.
 * @param tx буфер исходящего кадра. */
bool printDot(TxFrame& tx) noexcept {
    const uint8_t dot = static_cast<uint8_t>('.');
    return tx.pushBytes(&dot, 1u);
}

/** Печать запятой-разделителя между параметрами команды NIS.
 * @param tx буфер исходящего кадра. */
 bool printComma(TxFrame& tx) noexcept {
    const uint8_t comma = static_cast<uint8_t>(',');
    return tx.pushBytes(&comma, 1u);
}

/** Печать строки в двойных кавычках; экранирование `\r`, `\"`, `\\`.
 * @param tx буфер исходящего кадра.
 * @param text ASCIIZ содержимое; при nullptr записываются только открывающая и закрывающая кавычки. */
bool printQuotedString(TxFrame& tx, const char* text) noexcept {
    const uint8_t dq = static_cast<uint8_t>('"');
    if (!tx.pushBytes(&dq, 1u))
        return false;
    if (text == nullptr)
        return tx.pushBytes(&dq, 1u);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0u; ++p) {
        const unsigned char c = *p;
        if (c == static_cast<unsigned char>('"')) {
            static const char esc[] = "\\\"";
            if (!tx.pushBytes(esc, 2u))
                return false;
        } else if (c == static_cast<unsigned char>('\\')) {
            static const char esc[] = "\\\\";
            if (!tx.pushBytes(esc, 2u))
                return false;
        } else if (c == static_cast<unsigned char>('\r')) {
            static const char esc[] = "\\r";
            if (!tx.pushBytes(esc, 2u))
                return false;
        } else {
            if (!tx.pushBytes(&c, 1u))
                return false;
        }
    }
    return tx.pushBytes(&dq, 1u);
}

/** Печать беззнакового целого в десятичной записи без ведущих нулей (кроме нуля).
 * @param tx буфер исходящего кадра; байты добавляются в конец полезной нагрузки.
 * @param value число для вывода. */
bool printUint32(TxFrame& tx, uint32_t value) noexcept {
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

/** Печать знакового целого в десятичной записи (минус и модуль для отрицательных).
 * @param tx буфер исходящего кадра.
 * @param value число для вывода. */
bool printInt32(TxFrame& tx, int32_t value) noexcept {
    if (value >= 0)
        return printUint32(tx, static_cast<uint32_t>(value));
    const uint8_t minus = static_cast<uint8_t>('-');
    if (!tx.pushBytes(&minus, 1u))
        return false;
    uint32_t u = static_cast<uint32_t>(value);
    u = ~u + 1u;
    return printUint32(tx, u);
}

/** Печать оператора присваивания/модификации числового атрибута (`=`, `+=`, `-=`, …).
 * @param tx буфер исходящего кадра.
 * @param op вид операции из `cmd::assign::Numeric::Op`. */
bool printOperation(TxFrame& tx, cmd::assign::Numeric::Op op) noexcept {
    using Op = cmd::assign::Numeric::Op;
    switch (op) {
    case Op::Assign:
        return tx.pushBytes("=", 1u);
    case Op::Add:
        return tx.pushBytes("+=", 2u);
    case Op::Sub:
        return tx.pushBytes("-=", 2u);
    case Op::Mul:
        return tx.pushBytes("*=", 2u);
    case Op::Div:
        return tx.pushBytes("/=", 2u);
    case Op::Mod:
        return tx.pushBytes("%=", 2u);
    }
    return tx.pushBytes("=", 1u);
}

/** Печать лексемы атрибута: `attr`, `comp.attr` или только `attr` при `comp.len == 0` (полная левая часть).
 * @param tx буфер исходящего кадра.
 * @param t цель: `comp` и `attr` как `nex::Literal`. */
bool printAttrLexeme(TxFrame& tx, const cmd::TargetAttr& t) noexcept {
    if (t.attr.len == 0u)
        return false;
    if (t.comp.len == 0u)
        return printLiteral(tx, t.attr);
    return printLiteral(tx, t.comp) && printDot(tx) && printLiteral(tx, t.attr);
}

} // namespace misc

namespace cmd {
namespace assign {

bool Text::serialize(TxFrame& tx) const noexcept {
    if (!misc::printAttrLexeme(tx, _target))
        return false;
    using NO = Numeric::Op;
    const NO numOp = (_op == Op::Append) ? NO::Add : NO::Assign;
    if (!misc::printOperation(tx, numOp))
        return false;
    return misc::printQuotedString(tx, _text);
}

bool TextSubtract::serialize(TxFrame& tx) const noexcept {
    if (!misc::printAttrLexeme(tx, _target))
        return false;
    if (!misc::printOperation(tx, Numeric::Op::Sub))
        return false;
    return misc::printUint32(tx, _charsCount);
}

bool Numeric::serialize(TxFrame& tx) const noexcept {
    if (!misc::printAttrLexeme(tx, _target))
        return false;
    if (!misc::printOperation(tx, _op))
        return false;
    return misc::printInt32(tx, _value);
}

} // namespace assign

namespace oper {

// --- NIS §3 operational ---

bool BareOperCmd::serialize(TxFrame& tx) const noexcept {
    switch (_kind) {
    case Kind::TouchJ:
        return NEX_CMD_PRINT_LIT(tx, "touch_j");
    case Kind::Restart:
        return NEX_CMD_PRINT_LIT(tx, "rest");
    }
    return false;
}

bool Refresh::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "ref") || !misc::printSpace(tx))
        return false;
    return misc::printLiteral(tx, _compName);
}

bool RefreshWaveform::serialize(TxFrame& tx) const noexcept {
    if (_op == Op::Start)
        return NEX_CMD_PRINT_LIT(tx, "ref_start");
    return NEX_CMD_PRINT_LIT(tx, "ref_stop");
}

bool Get::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "get") || !misc::printSpace(tx))
        return false;
    return misc::printAttrLexeme(tx, _operand);
}

bool ConvertEx::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "covx") || !misc::printSpace(tx))
        return false;
    return misc::printAttrLexeme(tx, _src) && misc::printComma(tx) && misc::printAttrLexeme(tx, _dst) && misc::printComma(tx) &&
           misc::printInt32(tx, _length) && misc::printComma(tx) && misc::printInt32(tx, _format);
}

bool SubString::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "substr") || !misc::printSpace(tx))
        return false;
    return misc::printAttrLexeme(tx, _fromTxt) && misc::printComma(tx) && misc::printAttrLexeme(tx, _toTxt) && misc::printComma(tx) &&
           misc::printUint32(tx, _start) && misc::printComma(tx) && misc::printUint32(tx, _count);
}

bool Strlen::serialize(TxFrame& tx) const noexcept {
    if (_unit == Unit::Chars) {
        if (!NEX_CMD_PRINT_LIT(tx, "strlen") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "btlen") || !misc::printSpace(tx))
            return false;
    }
    return misc::printAttrLexeme(tx, _txtAttr) && misc::printComma(tx) && misc::printAttrLexeme(tx, _dstNumAttr);
}

bool SplitString::serialize(TxFrame& tx) const noexcept {
    if (_delimiterQuoted == nullptr)
        return false;
    if (!NEX_CMD_PRINT_LIT(tx, "spstr") || !misc::printSpace(tx))
        return false;
    const size_t ndq = std::strlen(_delimiterQuoted);
    if (ndq == 0u || ndq > static_cast<size_t>(UINT16_MAX))
        return false;
    return misc::printAttrLexeme(tx, _src) && misc::printComma(tx) && misc::printAttrLexeme(tx, _dst) && misc::printComma(tx) &&
           tx.pushBytes(_delimiterQuoted, static_cast<uint16_t>(ndq)) && misc::printComma(tx) && misc::printUint32(tx, _index);
}

bool CompLiteralUintCmd::serialize(TxFrame& tx) const noexcept {
    if (_kind == Kind::Vis) {
        if (!NEX_CMD_PRINT_LIT(tx, "vis") || !misc::printSpace(tx))
            return false;
    } else if (_kind == Kind::Tsw) {
        if (!NEX_CMD_PRINT_LIT(tx, "tsw") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "click") || !misc::printSpace(tx))
            return false;
    }
    return misc::printLiteral(tx, _compName) && misc::printComma(tx) && misc::printUint32(tx, _arg01);
}

bool Randset::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "randset") || !misc::printSpace(tx))
        return false;
    return misc::printInt32(tx, _min) && misc::printComma(tx) && misc::printInt32(tx, _max);
}

bool WaveAdd::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "add") || !misc::printSpace(tx))
        return false;
    return misc::printUint32(tx, _waveformId) && misc::printComma(tx) && misc::printUint32(tx, _channel) && misc::printComma(tx) &&
           misc::printUint32(tx, _value0to255);
}

bool WaveAddt::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "addt") || !misc::printSpace(tx))
        return false;
    return misc::printUint32(tx, _waveformId) && misc::printComma(tx) && misc::printUint32(tx, _channel) && misc::printComma(tx) &&
           misc::printUint32(tx, _byteCount);
}

bool WaveClear::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "cle") || !misc::printSpace(tx))
        return false;
    return misc::printUint32(tx, _waveformId) && misc::printComma(tx) && misc::printUint32(tx, _channelOr255All);
}

bool EepromVariable::serialize(TxFrame& tx) const noexcept {
    if (_op == Op::Write) {
        if (!NEX_CMD_PRINT_LIT(tx, "wepo") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "repo") || !misc::printSpace(tx))
            return false;
    }
    return misc::printAttrLexeme(tx, _target) && misc::printComma(tx) && misc::printUint32(tx, _addr);
}

bool EepromTransparent::serialize(TxFrame& tx) const noexcept {
    if (_op == Op::Write) {
        if (!NEX_CMD_PRINT_LIT(tx, "wept") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "rept") || !misc::printSpace(tx))
            return false;
    }
    return misc::printUint32(tx, _addr) && misc::printComma(tx) && misc::printUint32(tx, _byteCount);
}

bool Cfgpio::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "cfgpio") || !misc::printSpace(tx))
        return false;
    return misc::printUint32(tx, _pin) && misc::printComma(tx) && misc::printUint32(tx, _mode) && misc::printComma(tx) &&
           misc::printLiteral(tx, _bindCompNameOrZero);
}

bool Move::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "move") || !misc::printSpace(tx))
        return false;
    return misc::printLiteral(tx, _compName) && misc::printComma(tx) && misc::printInt32(tx, static_cast<int32_t>(_from.x)) &&
           misc::printComma(tx) && misc::printInt32(tx, static_cast<int32_t>(_from.y)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_to.x)) && misc::printComma(tx) && misc::printInt32(tx, static_cast<int32_t>(_to.y)) &&
           misc::printComma(tx) && misc::printUint32(tx, _priority) && misc::printComma(tx) && misc::printUint32(tx, _timeMs);
}

bool Play::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "play") || !misc::printSpace(tx))
        return false;
    return misc::printUint32(tx, _channel) && misc::printComma(tx) && misc::printUint32(tx, _resourceId) && misc::printComma(tx) &&
           misc::printUint32(tx, _loop01);
}

bool FsPathVerbCmd::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (_verb == Verb::DelFile) {
        if (!NEX_CMD_PRINT_LIT(tx, "delfile") || !misc::printSpace(tx))
            return false;
    } else if (_verb == Verb::DelDir) {
        if (!NEX_CMD_PRINT_LIT(tx, "deldir") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "newdir") || !misc::printSpace(tx))
            return false;
    }
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool FsRenameCmd::serialize(TxFrame& tx) const noexcept {
    if (_pathFromQuoted == nullptr || _pathToQuoted == nullptr)
        return false;
    if (_target == Target::File) {
        if (!NEX_CMD_PRINT_LIT(tx, "refile") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "redir") || !misc::printSpace(tx))
            return false;
    }
    const size_t na = std::strlen(_pathFromQuoted);
    const size_t nb = std::strlen(_pathToQuoted);
    if (na == 0u || nb == 0u || na > UINT16_MAX || nb > UINT16_MAX)
        return false;
    return tx.pushBytes(_pathFromQuoted, static_cast<uint16_t>(na)) && misc::printComma(tx) &&
           tx.pushBytes(_pathToQuoted, static_cast<uint16_t>(nb));
}

bool FsPathFindCmd::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (_target == Target::File) {
        if (!NEX_CMD_PRINT_LIT(tx, "findfile") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "finddir") || !misc::printSpace(tx))
            return false;
    }
    const size_t na = std::strlen(_pathQuoted);
    if (na == 0u || na > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(na)) && misc::printComma(tx) && misc::printAttrLexeme(tx, _dstNumAttr);
}

bool Rdfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!NEX_CMD_PRINT_LIT(tx, "rdfile") || !misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n)) && misc::printComma(tx) && misc::printUint32(tx, _offset) &&
           misc::printComma(tx) && misc::printUint32(tx, _count) && misc::printComma(tx) && misc::printUint32(tx, _crcOption);
}

bool Setlayer::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "setlayer") || !misc::printSpace(tx))
        return false;
    return misc::printLiteral(tx, _compName) && misc::printComma(tx) &&
           misc::printLiteral(tx, _aboveCompNameOr255);
}

bool Newfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!NEX_CMD_PRINT_LIT(tx, "newfile") || !misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n)) && misc::printComma(tx) && misc::printUint32(tx, _reservedSize);
}

bool Twfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!NEX_CMD_PRINT_LIT(tx, "twfile") || !misc::printSpace(tx))
        return false;
    const size_t np = std::strlen(_pathQuoted);
    if (np == 0u || np > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(np)) && misc::printComma(tx) && misc::printUint32(tx, _fileSize);
}

} // namespace oper

namespace gui {

bool ClearScreen::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "cls") || !misc::printSpace(tx))
        return false;
    return misc::printUint32(tx, static_cast<uint32_t>(_color.raw));
}

bool Picture::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "pic") || !misc::printSpace(tx))
        return false;
    return misc::printInt32(tx, static_cast<int32_t>(_at.x)) && misc::printComma(tx) && misc::printInt32(tx, static_cast<int32_t>(_at.y)) &&
           misc::printComma(tx) && misc::printUint32(tx, static_cast<uint32_t>(_pictureId));
}

bool PictureCropInPlace::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "picq") || !misc::printSpace(tx))
        return false;
    return misc::printInt32(tx, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, _w) && misc::printComma(tx) && misc::printUint32(tx, _h) && misc::printComma(tx) &&
           misc::printUint32(tx, static_cast<uint32_t>(_pictureId));
}

bool PictureCropDraw::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "xpic") || !misc::printSpace(tx))
        return false;
    return misc::printInt32(tx, static_cast<int32_t>(_dst.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_dst.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, _w) && misc::printComma(tx) && misc::printUint32(tx, _h) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_src.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_src.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, static_cast<uint32_t>(_pictureId));
}

bool TextInRegion::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "xstr") || !misc::printSpace(tx))
        return false;
    if (_contentToken == nullptr)
        return false;
    return misc::printInt32(tx, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, _w) && misc::printComma(tx) && misc::printUint32(tx, _h) && misc::printComma(tx) &&
           misc::printUint32(tx, _fontId) && misc::printComma(tx) && misc::printUint32(tx, static_cast<uint32_t>(_fg.raw)) &&
           misc::printComma(tx) && misc::printUint32(tx, static_cast<uint32_t>(_bg.raw)) && misc::printComma(tx) &&
           misc::printUint32(tx, _hAlign) && misc::printComma(tx) && misc::printUint32(tx, _vAlign) && misc::printComma(tx) &&
           misc::printUint32(tx, static_cast<uint32_t>(static_cast<uint8_t>(_fill))) && misc::printComma(tx) &&
           printNisToken(tx, _contentToken);
}

bool Rect::serialize(TxFrame& tx) const noexcept {
    const int32_t w = static_cast<int32_t>(_lowerRight.x) - static_cast<int32_t>(_upperLeft.x) + 1;
    const int32_t h = static_cast<int32_t>(_lowerRight.y) - static_cast<int32_t>(_upperLeft.y) + 1;
    if (w <= 0 || h <= 0)
        return false;

    if (_mode == Mode::Fill) {
        if (!NEX_CMD_PRINT_LIT(tx, "fill") || !misc::printSpace(tx))
            return false;
        return misc::printInt32(tx, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx) &&
               misc::printInt32(tx, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx) &&
               misc::printUint32(tx, static_cast<uint32_t>(w)) && misc::printComma(tx) &&
               misc::printUint32(tx, static_cast<uint32_t>(h)) && misc::printComma(tx) &&
               misc::printUint32(tx, static_cast<uint32_t>(_color.raw));
    }
    if (!NEX_CMD_PRINT_LIT(tx, "draw") || !misc::printSpace(tx))
        return false;
    return misc::printInt32(tx, static_cast<int32_t>(_upperLeft.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_upperLeft.y)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_lowerRight.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_lowerRight.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, static_cast<uint32_t>(_color.raw));
}

bool Line::serialize(TxFrame& tx) const noexcept {
    if (!NEX_CMD_PRINT_LIT(tx, "line") || !misc::printSpace(tx))
        return false;
    return misc::printInt32(tx, static_cast<int32_t>(_from.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_from.y)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_to.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_to.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, static_cast<uint32_t>(_color.raw));
}

bool Circle::serialize(TxFrame& tx) const noexcept {
    if (_kind == Kind::Filled) {
        if (!NEX_CMD_PRINT_LIT(tx, "cirs") || !misc::printSpace(tx))
            return false;
    } else {
        if (!NEX_CMD_PRINT_LIT(tx, "cir") || !misc::printSpace(tx))
            return false;
    }
    return misc::printInt32(tx, static_cast<int32_t>(_center.x)) && misc::printComma(tx) &&
           misc::printInt32(tx, static_cast<int32_t>(_center.y)) && misc::printComma(tx) &&
           misc::printUint32(tx, _radius) && misc::printComma(tx) && misc::printUint32(tx, static_cast<uint32_t>(_color.raw));
}

} // namespace gui

} // namespace cmd

} // namespace nex

#undef NEX_CMD_PRINT_LIT
