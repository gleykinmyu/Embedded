#pragma once

/**
 * @file fragment.hpp
 * @brief Zero-cost wrapper for LVGL fragment lifecycle management
 *
 * Provides RAII FragmentManager with push/pop navigation stack.
 * Fragment instances use C vtable pattern (lv_fragment_class_t).
 *
 * Usage:
 *   lv::FragmentManager mgr;
 *   auto* frag = lv::fragment::create(&my_fragment_class);
 *   mgr.push(frag, &container);
 */

#include <lvgl.h>

#if LV_USE_FRAGMENT

namespace lv {

/**
 * @brief RAII wrapper for the fragment manager
 *
 * Manages fragment lifecycle and navigation stack.
 *
 * Size: sizeof(void*) — pointer-sized
 */
class [[deprecated("Fragment is deprecated in LVGL 9.5. Use lv::Component instead.")]] FragmentManager {
    lv_fragment_manager_t* m_mgr = nullptr;

public:
    /// Create a fragment manager (optionally nested in a parent fragment)
    explicit FragmentManager(lv_fragment_t* parent = nullptr) noexcept
        : m_mgr(lv_fragment_manager_create(parent)) {}

    /// Adopt an existing manager
    struct adopt_t {};
    static constexpr adopt_t adopt{};
    FragmentManager(adopt_t, lv_fragment_manager_t* mgr) noexcept : m_mgr(mgr) {}

    ~FragmentManager() noexcept {
        if (m_mgr) lv_fragment_manager_delete(m_mgr);
    }

    // Move-only
    FragmentManager(const FragmentManager&) = delete;
    FragmentManager& operator=(const FragmentManager&) = delete;

    FragmentManager(FragmentManager&& other) noexcept : m_mgr(other.m_mgr) {
        other.m_mgr = nullptr;
    }

    FragmentManager& operator=(FragmentManager&& other) noexcept {
        if (this != &other) {
            if (m_mgr) lv_fragment_manager_delete(m_mgr);
            m_mgr = other.m_mgr;
            other.m_mgr = nullptr;
        }
        return *this;
    }

    // ==================== Fragment Management ====================

    /// Attach fragment to manager and add to container
    void add(lv_fragment_t* frag, lv_obj_t* const* container) noexcept {
        lv_fragment_manager_add(m_mgr, frag, container);
    }

    /// Detach and destroy fragment
    void remove(lv_fragment_t* frag) noexcept {
        lv_fragment_manager_remove(m_mgr, frag);
    }

    // ==================== Navigation Stack ====================

    /// Push fragment onto navigation stack
    void push(lv_fragment_t* frag, lv_obj_t* const* container) noexcept {
        lv_fragment_manager_push(m_mgr, frag, container);
    }

    /// Pop top fragment from stack
    bool pop() noexcept {
        return lv_fragment_manager_pop(m_mgr);
    }

    /// Replace top fragment in stack
    void replace(lv_fragment_t* frag, lv_obj_t* const* container) noexcept {
        lv_fragment_manager_replace(m_mgr, frag, container);
    }

    // ==================== Queries ====================

    /// Get navigation stack size
    [[nodiscard]] size_t stack_size() const noexcept {
        return lv_fragment_manager_get_stack_size(m_mgr);
    }

    /// Get top-most fragment
    [[nodiscard]] lv_fragment_t* top() const noexcept {
        return lv_fragment_manager_get_top(m_mgr);
    }

    /// Check if we can pop
    [[nodiscard]] bool can_pop() const noexcept {
        return lv_fragment_manager_get_stack_size(m_mgr) > 1;
    }

    /// Find first fragment in the given container
    [[nodiscard]] lv_fragment_t* find_by_container(const lv_obj_t* container) const noexcept {
        return lv_fragment_manager_find_by_container(m_mgr, container);
    }

    /// Get parent fragment (if this manager is nested)
    [[nodiscard]] lv_fragment_t* parent_fragment() const noexcept {
        return lv_fragment_manager_get_parent_fragment(m_mgr);
    }

    // ==================== Object Lifecycle ====================

    /// Create objects for all managed fragments
    void create_obj() noexcept {
        lv_fragment_manager_create_obj(m_mgr);
    }

    /// Delete objects (fragments remain alive)
    void delete_obj() noexcept {
        lv_fragment_manager_delete_obj(m_mgr);
    }

    // ==================== Events ====================

    /// Send event to top-most fragment
    bool send_event(int code, void* userdata = nullptr) noexcept {
        return lv_fragment_manager_send_event(m_mgr, code, userdata);
    }

    // ==================== Access ====================

    /// Get underlying manager pointer
    [[nodiscard]] lv_fragment_manager_t* get() noexcept { return m_mgr; }
    [[nodiscard]] const lv_fragment_manager_t* get() const noexcept { return m_mgr; }

    /// Check if valid
    [[nodiscard]] explicit operator bool() const noexcept { return m_mgr != nullptr; }
};

// ==================== Fragment Helpers ====================

namespace fragment {

/// Create a fragment instance from a class descriptor
inline lv_fragment_t* create(const lv_fragment_class_t* cls, void* args = nullptr) noexcept {
    return lv_fragment_create(cls, args);
}

/// Destroy a fragment instance
inline void destroy(lv_fragment_t* frag) noexcept {
    lv_fragment_delete(frag);
}

/// Create objects for a fragment
inline lv_obj_t* create_obj(lv_fragment_t* frag, lv_obj_t* container) noexcept {
    return lv_fragment_create_obj(frag, container);
}

/// Delete objects created by a fragment
inline void delete_obj(lv_fragment_t* frag) noexcept {
    lv_fragment_delete_obj(frag);
}

/// Recreate objects (destroy + create)
inline void recreate_obj(lv_fragment_t* frag) noexcept {
    lv_fragment_recreate_obj(frag);
}

/// Get the manager of a fragment
inline lv_fragment_manager_t* get_manager(lv_fragment_t* frag) noexcept {
    return lv_fragment_get_manager(frag);
}

/// Get the container of a fragment
inline lv_obj_t* const* get_container(lv_fragment_t* frag) noexcept {
    return lv_fragment_get_container(frag);
}

/// Get the parent fragment
inline lv_fragment_t* get_parent(lv_fragment_t* frag) noexcept {
    return lv_fragment_get_parent(frag);
}

} // namespace fragment

} // namespace lv

#endif // LV_USE_FRAGMENT
