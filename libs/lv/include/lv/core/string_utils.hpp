#pragma once

/**
 * @file string_utils.hpp
 * @brief String utilities for safe text handling with LVGL
 *
 * Provides helpers for safely passing std::string_view to LVGL's C API,
 * which requires null-terminated strings.
 */

#include <lvgl.h>
#include <string_view>
#include <cstring>
#include <cstddef>
#include <cstdarg>

namespace lv {

// ==================== String Formatting ====================

/// Cross-platform snprintf wrapper (uses LVGL's implementation for portability)
template<typename... Args>
inline int snprintf(char* buffer, size_t count, const char* format, Args... args) noexcept {
    return lv_snprintf(buffer, count, format, args...);
}

/**
 * @brief Execute a function with a null-terminated copy of a string_view
 *
 * Creates a temporary null-terminated string from the string_view and
 * passes it to the provided function. Uses stack allocation for small
 * strings to avoid heap allocation in most cases.
 *
 * @tparam Func Callable type that accepts const char*
 * @param sv The string_view (may not be null-terminated)
 * @param func Function to call with the null-terminated string
 *
 * Usage:
 *   lv::with_cstr(my_string_view, [&](const char* cstr) {
 *       lv_label_set_text(label, cstr);
 *   });
 */
template<typename Func>
inline void with_cstr(std::string_view sv, Func&& func) noexcept {
    constexpr size_t STACK_BUF_SIZE = 128;

    if (sv.size() < STACK_BUF_SIZE) {
        // Use stack buffer for small strings (common case)
        char buf[STACK_BUF_SIZE];
        std::memcpy(buf, sv.data(), sv.size());
        buf[sv.size()] = '\0';
        func(buf);
    } else {
        // Use heap for large strings
        char* buf = static_cast<char*>(lv_malloc(sv.size() + 1));
        if (buf) {
            std::memcpy(buf, sv.data(), sv.size());
            buf[sv.size()] = '\0';
            func(buf);
            lv_free(buf);
        }
    }
}

/**
 * @brief Convert string_view to null-terminated string in provided buffer
 *
 * For cases where you need direct control over the buffer.
 *
 * @param sv Source string_view
 * @param buf Destination buffer
 * @param buf_size Size of destination buffer
 * @return Pointer to buf, or nullptr if buffer too small
 */
inline const char* to_cstr(std::string_view sv, char* buf, size_t buf_size) noexcept {
    if (sv.size() >= buf_size) {
        return nullptr;  // Buffer too small
    }
    std::memcpy(buf, sv.data(), sv.size());
    buf[sv.size()] = '\0';
    return buf;
}

/**
 * @brief RAII wrapper for temporary null-terminated string
 *
 * Creates a null-terminated copy of a string_view that's valid for the
 * lifetime of this object. Useful when you need the string to persist
 * across multiple calls.
 *
 * Usage:
 *   lv::TempCStr temp(my_string_view);
 *   if (temp) {
 *       lv_label_set_text(label, temp.c_str());
 *       lv_obj_set_user_data(obj, (void*)temp.c_str());  // Still valid
 *   }
 */
class TempCStr {
    static constexpr size_t STACK_BUF_SIZE = 128;

    char m_stack_buf[STACK_BUF_SIZE];
    char* m_heap_buf = nullptr;
    const char* m_str = nullptr;

public:
    explicit TempCStr(std::string_view sv) noexcept {
        if (sv.size() < STACK_BUF_SIZE) {
            std::memcpy(m_stack_buf, sv.data(), sv.size());
            m_stack_buf[sv.size()] = '\0';
            m_str = m_stack_buf;
        } else {
            m_heap_buf = static_cast<char*>(lv_malloc(sv.size() + 1));
            if (m_heap_buf) {
                std::memcpy(m_heap_buf, sv.data(), sv.size());
                m_heap_buf[sv.size()] = '\0';
                m_str = m_heap_buf;
            }
        }
    }

    ~TempCStr() {
        if (m_heap_buf) {
            lv_free(m_heap_buf);
        }
    }

    // Non-copyable
    TempCStr(const TempCStr&) = delete;
    TempCStr& operator=(const TempCStr&) = delete;

    // Moveable
    TempCStr(TempCStr&& other) noexcept
        : m_heap_buf(other.m_heap_buf) {
        if (other.m_str == other.m_stack_buf) {
            std::memcpy(m_stack_buf, other.m_stack_buf, STACK_BUF_SIZE);
            m_str = m_stack_buf;
        } else {
            m_str = m_heap_buf;
        }
        other.m_heap_buf = nullptr;
        other.m_str = nullptr;
    }

    TempCStr& operator=(TempCStr&& other) noexcept {
        if (this != &other) {
            if (m_heap_buf) {
                lv_free(m_heap_buf);
            }
            m_heap_buf = other.m_heap_buf;
            if (other.m_str == other.m_stack_buf) {
                std::memcpy(m_stack_buf, other.m_stack_buf, STACK_BUF_SIZE);
                m_str = m_stack_buf;
            } else {
                m_str = m_heap_buf;
            }
            other.m_heap_buf = nullptr;
            other.m_str = nullptr;
        }
        return *this;
    }

    /// Get null-terminated C string
    [[nodiscard]] const char* c_str() const noexcept { return m_str; }
    [[nodiscard]] const char* get() const noexcept { return m_str; }

    /// Check if valid
    [[nodiscard]] bool valid() const noexcept { return m_str != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /// Implicit conversion to const char*
    operator const char*() const noexcept { return m_str; }
};

} // namespace lv
