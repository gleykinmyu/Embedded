#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace BIF{

namespace detail {
template<typename CharT, CharT... Cs>
struct LiteralStorage {
    inline static constexpr CharT value[sizeof...(Cs) + 1] = {Cs..., CharT{0}};
};
} // namespace detail

template<typename SizeT = std::size_t>
class SLiteral {
    public:
        using size_type = SizeT;

        static_assert(std::is_integral_v<size_type>, "SLiteral size type must be integral");
        static_assert(!std::is_same_v<size_type, bool>, "SLiteral size type must not be bool");
        static_assert(std::is_unsigned_v<size_type>, "SLiteral size type must be unsigned");

        SLiteral(const SLiteral&) = default;
        SLiteral& operator=(const SLiteral&) = default;
    
        constexpr const char* data() const noexcept { return _data; }
        constexpr size_type size() const noexcept { return _size; }

        constexpr bool operator==(const SLiteral& other) const noexcept {
            if (_size != other._size) {
                return false;
            }

            for (size_type i = 0; i < _size; ++i) {
                if (_data[i] != other._data[i]) {
                    return false;
                }
            }

            return true;
        }

        constexpr bool operator!=(const SLiteral& other) const noexcept {
            return !(*this == other);
        }

        constexpr bool operator<(const SLiteral& other) const noexcept {
            const size_type minSize = _size < other._size ? _size : other._size;

            for (size_type i = 0; i < minSize; ++i) {
                if (_data[i] < other._data[i]) {
                    return true;
                }

                if (_data[i] > other._data[i]) {
                    return false;
                }
            }

            return _size < other._size;
        }

        constexpr bool operator<=(const SLiteral& other) const noexcept {
            return !(other < *this);
        }

        constexpr bool operator>(const SLiteral& other) const noexcept {
            return other < *this;
        }

        constexpr bool operator>=(const SLiteral& other) const noexcept {
            return !(*this < other);
        }
    
    private:
        constexpr SLiteral(const char* data, size_type size) noexcept
            : _data(data), _size(size) {}

        template<typename CharT, CharT... Cs>
        friend constexpr auto operator""_sl() noexcept;

        const char* _data;
        size_type _size;
    };

    template<typename CharT, CharT... Cs>
    constexpr auto operator""_sl() noexcept {
        static_assert(std::is_same_v<CharT, char>, "Only narrow char literals are supported");

        constexpr std::size_t n = sizeof...(Cs);
        using literal_size_type = std::conditional_t<
            (n <= std::numeric_limits<std::uint8_t>::max()),
            std::uint8_t,
            std::conditional_t<
                (n <= std::numeric_limits<std::uint16_t>::max()),
                std::uint16_t,
                std::conditional_t<
                    (n <= std::numeric_limits<std::uint32_t>::max()),
                    std::uint32_t,
                    std::size_t
                >
            >
        >;

        return SLiteral<literal_size_type>(
            detail::LiteralStorage<CharT, Cs...>::value,
            static_cast<literal_size_type>(n)
        );
    }

    inline constexpr auto DefaultLiteral = "DefaultLiteral"_sl;
}