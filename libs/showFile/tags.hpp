/**
 * @file tags.hpp
 * @brief Тег секции (FourCC) и маски тегов TagMask / Tags<N>.
 */

#pragma once

#include <cstdint>
#include <type_traits>

namespace sf {

/** FourCC-тег секции шоуфайла; 0 — нет секции. */
using Tag = uint32_t;

/**
 * Маска секций: указатель на Tag[] + число элементов.
 * tags == nullptr → все секции Show; tags != nullptr && n == 0 → пусто (ошибка).
 * Хранилище массива — Tags<N>. Конструктор — protected (наследник / all).
 */
class TagMask {
public:
    const Tag* const tags;
    const uint8_t n;

    /** Все секции. */
    static const TagMask all;

    [[nodiscard]] bool contains(Tag tag) const noexcept
    {
        /* tags == nullptr — маска «все секции». */
        if (tags == nullptr) {
            return true;
        }
        /* Линейный поиск: масок на единицы тегов, сортировка не нужна. */
        for (uint8_t i = 0; i < n; ++i) {
            if (tags[i] == tag) {
                return true;
            }
        }
        return false;
    }
    [[nodiscard]] bool selectsAll() const noexcept { return tags == nullptr; }
    [[nodiscard]] bool empty() const noexcept { return tags != nullptr && n == 0u; }

protected:
    TagMask(const Tag* tags_, uint8_t n_) noexcept : tags(tags_), n(n_) {}
};

inline const TagMask TagMask::all{nullptr, 0};

/** Маска с собственным массивом Tag[N]. */
template <uint8_t N>
class Tags : public TagMask {
    static_assert(N > 0u, "Tags: N > 0");

public:
    Tags() noexcept : TagMask(_store, N) {}

    explicit Tags(const Tag (&src)[N]) noexcept : TagMask(_store, N)
    {
        for (uint8_t i = 0; i < N; ++i) {
            _store[i] = src[i];
        }
    }

    template <typename... Ts,
              typename = std::enable_if_t<(sizeof...(Ts) == N)
                                          && (std::is_convertible_v<Ts, Tag> && ...)>>
    Tags(Ts... tags) noexcept : TagMask(_store, N)
    {
        uint8_t i = 0;
        ((_store[i++] = static_cast<Tag>(tags)), ...);
    }

    Tags(const Tags& o) noexcept : TagMask(_store, N)
    {
        for (uint8_t i = 0; i < N; ++i) {
            _store[i] = o._store[i];
        }
    }
    Tags& operator=(const Tags& o) noexcept
    {
        if (this != &o) {
            for (uint8_t i = 0; i < N; ++i) {
                _store[i] = o._store[i];
            }
        }
        return *this;
    }
    Tags(Tags&&) = delete;
    Tags& operator=(Tags&&) = delete;

    [[nodiscard]] Tag& operator[](uint8_t i) noexcept { return _store[(i < N) ? i : 0u]; }
    [[nodiscard]] const Tag& operator[](uint8_t i) const noexcept
    {
        return _store[(i < N) ? i : 0u];
    }

private:
    Tag _store[N]{};
};

} // namespace sf
