#include "nexCommands.hpp"
#include <cstdint>
#include <cstring>

namespace {

/** Десятичные цифры без ведущих нулей в хвост `tx`. */
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

} // namespace

namespace nex
{

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

bool cmd::RawBytes::serialize(TxFrame& tx) const noexcept {
    if (_length > 0u && _text == nullptr)
        return false;
    if (_length > TxFrame::MAX_PAYLOAD)
        return false;
    return tx.pushBytes(_text, _length);
}

bool cmd::CString::serialize(TxFrame& tx) const noexcept {
    if (_z == nullptr)
        return false;
    const size_t n = std::strlen(_z);
    if (n > TxFrame::MAX_PAYLOAD || n > static_cast<size_t>(UINT16_MAX))
        return false;
    return tx.pushBytes(_z, static_cast<uint16_t>(n));
}

} // namespace nex
