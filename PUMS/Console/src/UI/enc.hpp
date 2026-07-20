/**
 * @file enc.hpp
 * @brief UTF-8 → OEM (FatFs `ff_convert`, сейчас KOI8-R / `_CODE_PAGE 20866`).
 *
 * Исходники и литералы в редакторе — UTF-8; на Nextion уходит однобайтовый OEM.
 * Имена с SD (FatFs API) уже в OEM — через этот конвертер их не пропускать.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace enc {

/**
 * UTF-8 → OEM (`ff_convert(..., 0)`).
 * @return число записанных байт без NUL; при нехватке места — усечение.
 * Неизвестные/битые последовательности → `'?'`.
 */
[[nodiscard]] std::size_t utf8ToOem(char* out, std::size_t outCap, const char* utf8) noexcept;

/** Стек-буфер: `msgBox.show(OemString("Файл"), …, "%s", OemString("Готово."));` */
class OemString {
public:
    static constexpr std::size_t kCap = 96u;

    explicit OemString(const char* utf8) noexcept;

    [[nodiscard]] const char* c_str() const noexcept { return _buf; }
    operator const char*() const noexcept { return _buf; }

private:
    char _buf[kCap]{};
};

} // namespace enc
