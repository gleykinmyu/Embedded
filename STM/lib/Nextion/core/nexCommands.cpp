#include "nexCommands.hpp"
#include <cstdint>
#include <cstring>

namespace nex {
namespace misc {

/** Печать литерала команды NIS (имя команды или ключевое слово) как сырой ASCII, без кавычек.
 * @param tx буфер исходящего кадра.
 * @param s ASCIIZ-строка; длина от 1 до UINT16_MAX байт полезной нагрузки. */
bool printLiteral(TxFrame& tx, const char* s) noexcept {
    if (s == nullptr)
        return false;
    const size_t n = std::strlen(s);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(s, static_cast<uint16_t>(n));
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

/** Печать лексемы компонента: `name` или `page.name` (без точки и имени атрибута).
 * @param tx буфер исходящего кадра.
 * @param c страница и имя компонента; оба nullptr — ошибка; только name — текущая страница. */
bool printCompLexeme(TxFrame& tx, const cmd::TargetComp& c) noexcept {
    if (c.page == nullptr && c.name == nullptr)
        return false;
    if (c.page == nullptr && c.name != nullptr) {
        const size_t nn = std::strlen(c.name);
        if (nn == 0u || nn > static_cast<size_t>(UINT16_MAX))
            return false;
        return tx.pushBytes(c.name, static_cast<uint16_t>(nn));
    }
    if (c.page != nullptr && c.name != nullptr) {
        const size_t np = std::strlen(c.page);
        const size_t nn = std::strlen(c.name);
        if (np == 0u || nn == 0u || np > static_cast<size_t>(UINT16_MAX) || nn > static_cast<size_t>(UINT16_MAX))
            return false;
        return tx.pushBytes(c.page, static_cast<uint16_t>(np)) && printDot(tx) &&
               tx.pushBytes(c.name, static_cast<uint16_t>(nn));
    }
    return false;
}

/** Печать лексемы атрибута: `attr`, `name.attr` или `page.name.attr` по правилам `TargetAttr` / `TargetComp`.
 * @param tx буфер исходящего кадра.
 * @param t цель: компонент (страница/имя) и имя атрибута; без компонента — только имя атрибута. */
bool printAttrLexeme(TxFrame& tx, const cmd::TargetAttr& t) noexcept {
    if (t.attr == nullptr)
        return false;
    const cmd::TargetComp& c = t.comp;
    if (c.page == nullptr && c.name == nullptr)
        return nex::misc::printLiteral(tx, t.attr);

    const size_t na = std::strlen(t.attr);
    if (na == 0u || na > static_cast<size_t>(UINT16_MAX))
        return false;
    return printCompLexeme(tx, c) && printDot(tx) && tx.pushBytes(t.attr, static_cast<uint16_t>(na));
}

} // namespace misc

namespace cmd {
namespace assign {

bool Text::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printAttrLexeme(tx, _target))
        return false;
    if (!nex::misc::printOperation(tx, Numeric::Op::Assign))
        return false;
    return nex::misc::printQuotedString(tx, text);
}

bool TextAppend::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printAttrLexeme(tx, _target))
        return false;
    if (!nex::misc::printOperation(tx, Numeric::Op::Add))
        return false;
    return nex::misc::printQuotedString(tx, text);
}

bool TextSubtract::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printAttrLexeme(tx, _target))
        return false;
    if (!nex::misc::printOperation(tx, Numeric::Op::Sub))
        return false;
    return nex::misc::printUint32(tx, _charsCount);
}

bool Numeric::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printAttrLexeme(tx, _target))
        return false;
    if (!nex::misc::printOperation(tx, _op))
        return false;
    return nex::misc::printInt32(tx, _value);
}

} // namespace assign

namespace oper {

// --- NIS §3 operational ---

bool ChangePage::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "page") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, static_cast<uint32_t>(_pageID));
}

bool Refresh::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "ref") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printCompLexeme(tx, _component);
}

bool RefreshStop::serialize(TxFrame& tx) const noexcept {
    return nex::misc::printLiteral(tx, "ref_stop");
}

bool RefreshStart::serialize(TxFrame& tx) const noexcept {
    return nex::misc::printLiteral(tx, "ref_start");
}

bool Get::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "get") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printAttrLexeme(tx, _operand);
}

bool GetCurrentPageID::serialize(TxFrame& tx) const noexcept {
    return nex::misc::printLiteral(tx, "sendme");
}

bool ConvertEx::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "covx") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printAttrLexeme(tx, _src) && nex::misc::printComma(tx) && nex::misc::printAttrLexeme(tx, _dst) && nex::misc::printComma(tx) &&
           nex::misc::printInt32(tx, _length) && nex::misc::printComma(tx) && nex::misc::printInt32(tx, _format);
}

bool SubStr::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "substr") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printAttrLexeme(tx, _fromTxt) && nex::misc::printComma(tx) && nex::misc::printAttrLexeme(tx, _toTxt) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, _start) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _count);
}

bool Strlen::serialize(TxFrame& tx) const noexcept {
    const char* lit = (_unit == Unit::Chars) ? "strlen" : "btlen";
    if (!nex::misc::printLiteral(tx, lit) || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printAttrLexeme(tx, _txtAttr) && nex::misc::printComma(tx) && nex::misc::printAttrLexeme(tx, _dstNumAttr);
}

bool Spstr::serialize(TxFrame& tx) const noexcept {
    if (_delimiterQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "spstr") || !nex::misc::printSpace(tx))
        return false;
    const size_t ndq = std::strlen(_delimiterQuoted);
    if (ndq == 0u || ndq > static_cast<size_t>(UINT16_MAX))
        return false;
    return nex::misc::printAttrLexeme(tx, _src) && nex::misc::printComma(tx) && nex::misc::printAttrLexeme(tx, _dst) && nex::misc::printComma(tx) &&
           tx.pushBytes(_delimiterQuoted, static_cast<uint16_t>(ndq)) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _index);
}

bool TouchJ::serialize(TxFrame& tx) const noexcept {
    return nex::misc::printLiteral(tx, "touch_j");
}

bool Visible::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "vis") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printCompLexeme(tx, _component) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, static_cast<uint32_t>(_state));
}

bool TouchSwitch::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "tsw") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printCompLexeme(tx, _component) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, static_cast<uint32_t>(_enabled));
}

bool Click::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "click") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printCompLexeme(tx, _component) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, static_cast<uint32_t>(_press1Release0));
}

bool Randset::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "randset") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printInt32(tx, _min) && nex::misc::printComma(tx) && nex::misc::printInt32(tx, _max);
}

bool WaveAdd::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "add") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, _waveformId) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _channel) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, _value0to255);
}

bool WaveAddt::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "addt") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, _waveformId) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _channel) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, _byteCount);
}

bool WaveCle::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "cle") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, _waveformId) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _channelOr255All);
}

bool Rest::serialize(TxFrame& tx) const noexcept {
    return nex::misc::printLiteral(tx, "rest");
}

bool DoEvents::serialize(TxFrame& tx) const noexcept {
    return nex::misc::printLiteral(tx, "doevents");
}

bool EepromVariable::serialize(TxFrame& tx) const noexcept {
    const char* lit = (_op == Op::Write) ? "wepo" : "repo";
    if (!nex::misc::printLiteral(tx, lit) || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printAttrLexeme(tx, _target) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _addr);
}

bool EepromTransparent::serialize(TxFrame& tx) const noexcept {
    const char* lit = (_op == Op::Write) ? "wept" : "rept";
    if (!nex::misc::printLiteral(tx, lit) || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, _addr) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _byteCount);
}

bool Cfgpio::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "cfgpio") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, _pin) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _mode) && nex::misc::printComma(tx) &&
           nex::misc::printCompLexeme(tx, _bindComponentOrZero);
}

bool Move::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "move") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printCompLexeme(tx, _component) && nex::misc::printComma(tx) && nex::misc::printInt32(tx, _x0) && nex::misc::printComma(tx) &&
           nex::misc::printInt32(tx, _y0) && nex::misc::printComma(tx) && nex::misc::printInt32(tx, _x1) && nex::misc::printComma(tx) && nex::misc::printInt32(tx, _y1) &&
           nex::misc::printComma(tx) && nex::misc::printUint32(tx, _priority) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _timeMs);
}

bool Play::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "play") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printUint32(tx, _channel) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _resourceId) && nex::misc::printComma(tx) &&
           nex::misc::printUint32(tx, _loop01);
}

bool Twfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "twfile") || !nex::misc::printSpace(tx))
        return false;
    const size_t np = std::strlen(_pathQuoted);
    if (np == 0u || np > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(np)) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _fileSize);
}

bool Delfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "delfile") || !nex::misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool Refile::serialize(TxFrame& tx) const noexcept {
    if (_pathFromQuoted == nullptr || _pathToQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "refile") || !nex::misc::printSpace(tx))
        return false;
    const size_t na = std::strlen(_pathFromQuoted);
    const size_t nb = std::strlen(_pathToQuoted);
    if (na == 0u || nb == 0u || na > UINT16_MAX || nb > UINT16_MAX)
        return false;
    return tx.pushBytes(_pathFromQuoted, static_cast<uint16_t>(na)) && nex::misc::printComma(tx) &&
           tx.pushBytes(_pathToQuoted, static_cast<uint16_t>(nb));
}

bool Findfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "findfile") || !nex::misc::printSpace(tx))
        return false;
    const size_t na = std::strlen(_pathQuoted);
    if (na == 0u || na > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(na)) && nex::misc::printComma(tx) && nex::misc::printAttrLexeme(tx, _dstNumAttr);
}

bool Rdfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "rdfile") || !nex::misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n)) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _offset) &&
           nex::misc::printComma(tx) && nex::misc::printUint32(tx, _count) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _crcOption);
}

bool Setlayer::serialize(TxFrame& tx) const noexcept {
    if (!nex::misc::printLiteral(tx, "setlayer") || !nex::misc::printSpace(tx))
        return false;
    return nex::misc::printCompLexeme(tx, _component) && nex::misc::printComma(tx) &&
           nex::misc::printCompLexeme(tx, _aboveOr255);
}

bool Newdir::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "newdir") || !nex::misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool Deldir::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "deldir") || !nex::misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n));
}

bool Redir::serialize(TxFrame& tx) const noexcept {
    if (_pathFromQuoted == nullptr || _pathToQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "redir") || !nex::misc::printSpace(tx))
        return false;
    const size_t na = std::strlen(_pathFromQuoted);
    const size_t nb = std::strlen(_pathToQuoted);
    if (na == 0u || nb == 0u || na > UINT16_MAX || nb > UINT16_MAX)
        return false;
    return tx.pushBytes(_pathFromQuoted, static_cast<uint16_t>(na)) && nex::misc::printComma(tx) &&
           tx.pushBytes(_pathToQuoted, static_cast<uint16_t>(nb));
}

bool Finddir::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "finddir") || !nex::misc::printSpace(tx))
        return false;
    const size_t na = std::strlen(_pathQuoted);
    if (na == 0u || na > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(na)) && nex::misc::printComma(tx) && nex::misc::printAttrLexeme(tx, _dstNumAttr);
}

bool Newfile::serialize(TxFrame& tx) const noexcept {
    if (_pathQuoted == nullptr)
        return false;
    if (!nex::misc::printLiteral(tx, "newfile") || !nex::misc::printSpace(tx))
        return false;
    const size_t n = std::strlen(_pathQuoted);
    if (n == 0u || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_pathQuoted, static_cast<uint16_t>(n)) && nex::misc::printComma(tx) && nex::misc::printUint32(tx, _reservedSize);
}

} // namespace oper

} // namespace cmd

} // namespace nex
