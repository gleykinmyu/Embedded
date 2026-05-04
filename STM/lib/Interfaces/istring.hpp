#pragma once

#include <cstddef>
#include <cstring>

namespace BIF {

/// Строка: одна сущность с const- и не-const-доступом.
/// cap_ == 0 — только чтение (литерал во flash); мутабельные методы — no-op.
/// Литерал всегда отдаётся как `const String&` из UDL — не-const методы недоступны.
class String {
public:
    constexpr String(const char* p, std::size_t sz, std::size_t cap) noexcept
        : ptr_(p), size_(sz), cap_(cap) {}

    constexpr const char* data() const noexcept { return ptr_; }

    char* data() noexcept { return cap_ ? const_cast<char*>(ptr_) : nullptr; }

    constexpr std::size_t size() const noexcept { return size_; }

    constexpr std::size_t capacity() const noexcept { return cap_; }

    constexpr bool is_mutable() const noexcept { return cap_ != 0; }

    void clear() noexcept;
    void set(const char* str) noexcept;
    void append(const char* str) noexcept;

private:
    const char* ptr_;
    std::size_t size_;
    std::size_t cap_;
};

inline void String::clear() noexcept {
    if (cap_ == 0) {
        return;
    }
    char* p = const_cast<char*>(ptr_);
    p[0] = '\0';
    size_ = 0;
}

inline void String::set(const char* str) noexcept {
    if (cap_ == 0 || !str) {
        return;
    }
    char* p = const_cast<char*>(ptr_);
    std::strncpy(p, str, cap_ - 1);
    p[cap_ - 1] = '\0';
    size_ = std::strlen(p);
}

inline void String::append(const char* str) noexcept {
    if (cap_ == 0 || !str) {
        return;
    }
    char* p = const_cast<char*>(ptr_);
    if (size_ >= cap_ - 1) {
        return;
    }
    std::strncat(p, str, cap_ - size_ - 1);
    size_ = std::strlen(p);
}

// ---------------------------------------------------------------------------
// Буфер в RAM: `buf_` инициализируется до `str_`, указатель в String валиден.
// ---------------------------------------------------------------------------

template<std::size_t Capacity>
class RamString {
    static_assert(Capacity > 1, "RamString: Capacity must be > 1");

    char buf_[Capacity]{};
    String str_;

public:
    RamString() noexcept : str_(buf_, 0, Capacity) {}

    String& string() noexcept { return str_; }

    const String& string() const noexcept { return str_; }

    operator String&() noexcept { return str_; }

    operator const String&() const noexcept { return str_; }
};

} // namespace BIF
