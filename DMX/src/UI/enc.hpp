/**
 * @file enc.hpp
 * @brief UTF-8 → OEM только для текстовых надписей (`xstr` / `.txt`).
 *
 * Команды протокола, числа и ASCII не перекодировать.
 * FatFs `ff_convert`, сейчас KOI8-R / `_CODE_PAGE 20866`.
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
std::size_t utf8ToOem(char* out, std::size_t outCap, const char* utf8) noexcept;

} // namespace enc
