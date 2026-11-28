/**
 * @file bitsery/custom/extensions.hpp
 * @brief putBits — поле фиксированной разрядности через ValueRange.
 *
 * putBits<N> задаёт ширину числом бит. Потолок поля — 2^N-1, не последний enum.
 * putBits(v, min, max) остаётся для произвольного диапазона.
 *
 * На записи значение вне диапазона — msb::WriteError::InvalidValue.
 * Проверка остаётся в Release: на assert Bitsery она не опирается.
 * Уже записанная ошибка адаптера не заменяется.
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include <bitsery/ext/value_range.h>

#include "bitsery/custom/serializer.hpp"

namespace bitsery::custom {

namespace detail {

template <typename A, typename = void>
struct is_writer : std::false_type {};

template <typename A>
struct is_writer<
    A,
    std::void_t<decltype(std::declval<A&>().template writeBytes<1>(std::uint8_t{}))>>
    : std::true_type {};

template <typename T>
constexpr auto asInt(T v) noexcept
{
    if constexpr (std::is_enum<T>::value) {
        return static_cast<std::underlying_type_t<T>>(v);
    } else {
        return v;
    }
}

template <typename T, bool IsEnum>
struct store;

template <typename T>
struct store<T, true> {
    using type = std::underlying_type_t<T>;
};

template <typename T>
struct store<T, false> {
    using type = T;
};

template <typename T>
using store_t = typename store<T, std::is_enum<T>::value>::type;

template <typename T, size_t Bits>
constexpr T fieldMax() noexcept
{
    using U = store_t<T>;
    static_assert(std::is_unsigned<U>::value, "putBits<N> takes an unsigned value or enum");
    static_assert(Bits > 0u, "putBits<N>: width is zero");
    static_assert(Bits <= sizeof(U) * 8u, "putBits<N>: width exceeds the type");
    if constexpr (Bits == sizeof(U) * 8u) {
        return static_cast<T>(static_cast<U>(~U{0}));
    } else {
        return static_cast<T>(static_cast<U>((1ull << Bits) - 1ull));
    }
}

} // namespace detail

template <typename S>
inline constexpr bool isWriter = detail::is_writer<
    std::remove_reference_t<decltype(std::declval<S&>().adapter())>>::value;

template <typename S, typename T>
void putBits(S& s, T& v, T min, T max)
{
    if constexpr (isWriter<S>) {
        const auto iv = detail::asInt(v);
        const auto imin = detail::asInt(min);
        const auto imax = detail::asInt(max);
        if (imax < imin || iv < imin || iv > imax) {
            s.adapter().error(msb::WriteError::InvalidValue);
            return;
        }
    }
    s.ext(v, bitsery::ext::ValueRange<T>{min, max});
}

template <typename S, typename T>
void putBits(S& s, T& v, T max)
{
    putBits(s, v, T{}, max);
}

/** Ровно Bits бит. Максимум — 2^Bits-1, ширина не зависит от имён enum. */
template <size_t Bits, typename S, typename T>
void putBits(S& s, T& value)
{
    constexpr T max = detail::fieldMax<T, Bits>();
    putBits(s, value, T{}, max);
}

} // namespace bitsery::custom
