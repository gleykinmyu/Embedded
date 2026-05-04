#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "istring.hpp"

namespace BIF {

namespace detail {

template<typename CharT, CharT... Cs>
struct LiteralStorage {
    struct Blob {
        std::uint16_t length;
        CharT str[sizeof...(Cs) + 1];
    };

    static constexpr Blob value{
        static_cast<std::uint16_t>(sizeof...(Cs)),
        {Cs..., CharT{0}},
    };

    static_assert(std::is_standard_layout_v<Blob>);
    static_assert(offsetof(Blob, str) == sizeof(std::uint16_t));
};

} // namespace detail

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

template<typename CharT, CharT... Cs>
inline const String& operator""_sl() noexcept {
    static_assert(std::is_same_v<CharT, char>, "Only narrow char literals are supported");
    static_assert(sizeof...(Cs) <= UINT16_MAX, "String literal exceeds uint16_t length");

    static constexpr auto blob = detail::LiteralStorage<CharT, Cs...>::value;
    static const String view(
        blob.str,
        static_cast<std::size_t>(blob.length),
        0
    );
    return view;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

inline const String& DefaultLiteral = "DefaultLiteral"_sl;

} // namespace BIF
