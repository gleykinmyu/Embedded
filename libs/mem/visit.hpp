/**
 * @file mem/visit.hpp
 * @brief mem::detail::visit — вызов обходчика forEach: fn → bool (true — стоп) или void.
 */

#pragma once

#include <type_traits>

namespace mem::detail {

/** fn(args…) → bool или void; иначе — ошибка компиляции. @return true — стоп. */
template <typename Fn, typename... A>
constexpr bool visit(Fn& fn, A&&... a) noexcept(noexcept(fn(static_cast<A&&>(a)...)))
{
    using R = std::invoke_result_t<Fn&, A...>;
    static_assert(std::is_void_v<R> || std::is_same_v<R, bool>,
                  "forEach: fn must return void or bool");
    if constexpr (std::is_same_v<R, bool>) {
        return fn(static_cast<A&&>(a)...);
    } else {
        fn(static_cast<A&&>(a)...);
        return false;
    }
}

} // namespace mem::detail
