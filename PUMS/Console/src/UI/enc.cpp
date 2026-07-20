#include "enc.hpp"

#include "ff.h"

namespace enc {
namespace {

/** Один Unicode scalar (BMP) из UTF-8; @a p сдвигается. 0 — ошибка/конец. */
[[nodiscard]] uint32_t utf8Next(const char*& p) noexcept
{
    if (p == nullptr || *p == '\0') {
        return 0u;
    }

    const auto b0 = static_cast<uint8_t>(*p++);
    if (b0 < 0x80u) {
        return b0;
    }

    /* 2 байта: U+0080..U+07FF (кириллица сюда) */
    if ((b0 & 0xE0u) == 0xC0u) {
        const auto b1 = static_cast<uint8_t>(*p);
        if ((b1 & 0xC0u) != 0x80u) {
            return 0u;
        }
        ++p;
        const uint32_t cp = (static_cast<uint32_t>(b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
        return (cp >= 0x80u) ? cp : 0u;
    }

    /* 3 байта: U+0800..U+FFFF */
    if ((b0 & 0xF0u) == 0xE0u) {
        const auto b1 = static_cast<uint8_t>(p[0]);
        const auto b2 = static_cast<uint8_t>(p[1]);
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) {
            return 0u;
        }
        p += 2;
        const uint32_t cp = (static_cast<uint32_t>(b0 & 0x0Fu) << 12)
            | (static_cast<uint32_t>(b1 & 0x3Fu) << 6)
            | (b2 & 0x3Fu);
        return (cp >= 0x800u) ? cp : 0u;
    }

    /* 4 байта / мусор — пропускаем ведущий и дальше по байту в вызывающем. */
    return 0u;
}

[[nodiscard]] char unicodeToOem(uint32_t cp) noexcept
{
    if (cp == 0u || cp > 0xFFFFu) {
        return '?';
    }
    const WCHAR oem = ff_convert(static_cast<WCHAR>(cp), 0u);
    if (oem == 0 && cp != 0u) {
        return '?';
    }
    return static_cast<char>(static_cast<unsigned char>(oem & 0xFFu));
}

} // namespace

std::size_t utf8ToOem(char* out, std::size_t outCap, const char* utf8) noexcept
{
    if (out == nullptr || outCap == 0u) {
        return 0u;
    }
    if (utf8 == nullptr) {
        out[0] = '\0';
        return 0u;
    }

    std::size_t n = 0u;
    const char* p = utf8;
    while (*p != '\0' && (n + 1u) < outCap) {
        const char* before = p;
        const uint32_t cp = utf8Next(p);
        if (cp == 0u) {
            /* Битый UTF-8: съесть один байт, поставить '?'. */
            if (p == before) {
                ++p;
            }
            out[n++] = '?';
            continue;
        }
        out[n++] = unicodeToOem(cp);
    }
    out[n] = '\0';
    return n;
}

OemString::OemString(const char* utf8) noexcept
{
    (void)utf8ToOem(_buf, kCap, utf8);
}

} // namespace enc
