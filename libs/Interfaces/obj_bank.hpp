/**
 * @file obj_bank.hpp
 * @brief Банк объектов T[N] с общим owner; слоты 0..N-1.
 *
 * Indexed=true  — T{args..., uint8_t(i)}
 * Indexed=false — T{args...} (id не передаётся, как Session → Node::sessions()).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace MISC {

template <typename T, uint8_t N, bool Indexed = true>
class ObjBank {
    static_assert(N > 0u, "ObjBank: N must be > 0");

public:
    static constexpr uint8_t kCount = N;

    template <typename... Args>
    explicit ObjBank(Args&... args) noexcept
        : ObjBank(std::bool_constant<Indexed>{}, std::make_index_sequence<N>{}, args...)
    {}

    ObjBank(const ObjBank&) = delete;
    ObjBank& operator=(const ObjBank&) = delete;
    ObjBank(ObjBank&&) = delete;
    ObjBank& operator=(ObjBank&&) = delete;

    [[nodiscard]] T& operator[](uint8_t i) noexcept { return _items[i]; }
    [[nodiscard]] const T& operator[](uint8_t i) const noexcept { return _items[i]; }

    [[nodiscard]] T* begin() noexcept { return _items; }
    [[nodiscard]] T* end() noexcept { return _items + N; }
    [[nodiscard]] const T* begin() const noexcept { return _items; }
    [[nodiscard]] const T* end() const noexcept { return _items + N; }

    [[nodiscard]] static constexpr uint8_t size() noexcept { return N; }

private:
    template <std::size_t... I, typename... Args>
    ObjBank(std::true_type, std::index_sequence<I...>, Args&... args) noexcept
        : _items{T{args..., static_cast<uint8_t>(I)}...}
    {}

    template <std::size_t... I, typename... Args>
    ObjBank(std::false_type, std::index_sequence<I...>, Args&... args) noexcept
        : _items{((void)I, T{args...})...}
    {}

    T _items[N];
};

} // namespace MISC
